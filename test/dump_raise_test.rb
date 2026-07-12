#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.dump output buffer leak on the dump-side raise paths.
#
# dump_obj_to_xml() allocates out->buf with ALLOC_N and the caller frees it only
# on the normal return. Before the fix, any rb_raise during the traversal (an
# invalid character, an oversized :indent, or the deep-recursion SystemStackError
# from a circular reference) longjmped past that free and leaked the whole buffer
# (plus any grow() reallocations).
#
# The leak itself is caught by Valgrind memcheck (rake test:valgrind). This file
# is the plain-Ruby guard for the same fix: every dump-side raise must still raise
# the right error, and -- the part that would break if the cleanup were wrong --
# ox must remain in a healthy state afterward, so a normal dump right after a
# raise still produces correct output.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpRaiseTest < ::Test::Unit::TestCase
  def test_invalid_character_still_raises_and_recovers
    100.times do
      assert_raise(Ox::SyntaxError) { Ox.dump("bad\x19char") }
    end
    # The buffer cleanup must leave ox able to dump normally afterward.
    assert_equal('ok', Ox.load(Ox.dump('ok', mode: :object), mode: :object))
  end

  def test_oversized_indent_still_raises_and_recovers
    100.times do
      assert_raise(Ox::ParseError) { Ox.dump([1], indent: 10_000_000) }
    end
    assert_equal([1, 2], Ox.load(Ox.dump([1, 2], mode: :object, indent: 2), mode: :object))
  end

  def test_circular_reference_still_raises_and_recovers
    10.times do
      a = []
      a << a
      assert_raise(SystemStackError) { Ox.dump(a, mode: :object) }
    end
    # circular: true is the supported path and must still succeed.
    a = []
    a << a
    assert_nothing_raised { Ox.dump(a, mode: :object, circular: true) }
  end

  # Interleave dump-side raises with a forced GC and normal dumps. A buffer that
  # was freed twice, or left dangling, would surface here as a crash.
  def test_raise_then_gc_then_dump
    sink = []
    200.times do |i|
      begin
        Ox.dump("x\x19#{i}")
      rescue Ox::SyntaxError
        # expected
      end
      GC.start(full_mark: true, immediate_sweep: true)
      sink << Ox.load(Ox.dump({ 'i' => i }, mode: :object), mode: :object)
    end
    assert_equal(200, sink.size)
    assert_equal({ 'i' => 199 }, sink.last)
  end
end
