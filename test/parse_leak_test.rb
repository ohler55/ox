#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: memory leaks on the parse-side error-return paths.
#
# Two allocations in parse.c were released only on some of the paths that leave
# the function:
#
#   * read_instruction() heap allocates content_ptr when a processing
#     instruction's body reaches 256 bytes, but five of its error returns left
#     without the xfree() that only the success path performed. The body length
#     is attacker chosen, so each malformed document leaked that much heap.
#   * read_element() moves the attribute stack to the heap past
#     ATTR_STACK_INC (8) attributes, and two of its error returns -- the
#     read_quoted_value() failure and the "a child element errored" unwind --
#     omitted attr_stack_cleanup(). One block leaked per nesting level.
#
# Both leaked once per parse and were invisible to Ruby's GC, so a service
# parsing untrusted XML grew until the process was OOM killed while the parse
# itself just raised Ox::ParseError.
#
# The leaks themselves are caught by Valgrind memcheck (rake test:valgrind).
# This file is the plain-Ruby guard for the same fix: every one of those paths
# must still raise the same error, and ox must remain healthy afterward, which
# is what would break if a cleanup freed something it should not.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ParseLeakTest < ::Test::Unit::TestCase
  # Comfortably past the 256-byte stack buffer in read_instruction, so
  # content_ptr is the heap allocation on every one of these documents.
  PI_BODY = 'a' * 400

  # More than ATTR_STACK_INC (8), so the attribute stack is heap grown.
  ATTRS = (1..9).map { |i| "a#{i}=\"#{i}\"" }.join(' ')
  CHILD_ATTRS = (1..9).map { |i| "b#{i}=\"#{i}\"" }.join(' ')

  def assert_still_healthy
    assert_equal({ 'k' => 1 }, Ox.load(Ox.dump({ 'k' => 1 }, mode: :object), mode: :object))
  end

  # read_instruction: the attribute loop finds the document terminated. The name
  # token stops at '=', so *pi->s is the NUL that "terminate name" just wrote.
  def test_instruction_unterminated_after_long_body
    xml = "<?x=#{PI_BODY}?>"
    200.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # read_instruction: read_quoted_value() fails on an unterminated value after
  # the body was already heap allocated.
  def test_instruction_unterminated_attr_value
    xml = "<?xml v=\"#{PI_BODY}?>"
    200.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # The pre-allocation exit (no '?>' anywhere) must keep behaving too -- it is
  # now routed through the same cleanup label with content_ptr still pointing at
  # the stack buffer, so a wrong fix here would free stack memory.
  def test_instruction_never_terminated
    xml = "<?xml #{PI_BODY}"
    200.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # read_element: read_quoted_value() fails after the attribute stack moved to
  # the heap.
  def test_element_unterminated_attr_value
    xml = "<r #{ATTRS} a10=\"oops"
    200.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # read_element: a child element errors and the parent unwinds through the
  # err_has() branch, which is the second path that skipped the cleanup. Both
  # the parent's and the child's attribute stacks are heap grown here.
  def test_element_child_error_unwinds_parent
    xml = "<r #{ATTRS}><c #{CHILD_ATTRS} b10=\"oops"
    200.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # One block leaked per nesting level, so drive the unwind through several
  # levels that each hold a heap-grown attribute stack.
  def test_element_deep_child_error_unwinds_every_level
    xml = "<r #{ATTRS}>" + (1..20).map { |i| "<m#{i} #{ATTRS}>" }.join + "<c #{CHILD_ATTRS} b10=\"oops"
    100.times do
      assert_raise(Ox::ParseError) { Ox.load(xml) }
    end
    assert_still_healthy
  end

  # Well-formed instructions and many-attribute elements must be unaffected --
  # this is the success path through the same cleanup label.
  def test_valid_instruction_and_attributes_unchanged
    doc = Ox.parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?><r #{ATTRS}><c/></r>")
    assert_equal('r', doc.root.name)
    assert_equal('1', doc.root.attributes[:a1])
    assert_equal('9', doc.root.attributes[:a9])
    assert_equal(9, doc.root.attributes.size)

    # A long but valid processing instruction: the heap content buffer is taken
    # and released on the success path.
    long = Ox.parse("<?big #{PI_BODY}?><r/>")
    assert_equal('r', long.root.name)
  end

  # Interleave the error paths with a forced GC and normal parses. A cleanup
  # that freed the wrong pointer, or freed twice, would surface here as a crash.
  def test_error_paths_then_gc_then_parse
    sink = []
    inputs = ["<?x=#{PI_BODY}?>",
              "<?xml v=\"#{PI_BODY}?>",
              "<r #{ATTRS} a10=\"oops",
              "<r #{ATTRS}><c #{CHILD_ATTRS} b10=\"oops"]
    200.times do |i|
      begin
        Ox.load(inputs[i % inputs.size])
      rescue Ox::ParseError
        # expected
      end
      GC.start(full_mark: true, immediate_sweep: true)
      sink << Ox.parse("<?xml version=\"1.0\"?><r #{ATTRS}><c>#{i}</c></r>").root.locate('c/^Text').first
    end
    assert_equal(200, sink.size)
    assert_equal('199', sink.last)
  end
end
