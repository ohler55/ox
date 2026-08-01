#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: read_from_fd() reporting EOF as a successful read.
#
# When read() returns 0 the function left read_end where it was and returned 0,
# so buf_get() fell through and handed back *buf->tail anyway, pushing tail past
# read_end. Every later buf_get() then returned bytes read() never wrote --
# whatever the previous parse left in the 4096 byte buf.base array, which lives
# in the _saxDrive struct on the C stack.
#
# It is reachable because parse() reads seven characters unconditionally after
# "<!", and read_name_token() keeps going after that. A three byte file is
# enough. read_from_str() already returns non-zero at EOF for this reason; the
# fd path now does the same.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tempfile'
require 'ox'

class SaxEofTest < ::Test::Unit::TestCase
  class Collect < ::Ox::Sax
    attr_reader :seen

    def initialize
      @seen = []
    end

    def start_element(name)
      @seen << name.to_s
    end

    def end_element(name)
      @seen << name.to_s
    end

    def text(value)
      @seen << value.to_s
    end

    def attr(name, value)
      @seen << "#{name}=#{value}"
    end

    def error(_message, _line, _column); end
  end

  MARKER = 'CANARY_LEAK_TEXT'

  def sax_file(body)
    handler = Collect.new
    Tempfile.create(['sax_eof', '.xml']) do |f|
      f.write(body)
      f.flush
      File.open(f.path) { |io| Ox.sax_parse(handler, io) }
    end
    handler.seen
  end

  # The marker has to reach buf.base and it needs a '>' in front of it, because
  # the over-read resumes inside read_name_token() and that is what ends the
  # token it is scanning.
  def prime_the_buffer
    sax_file("<top><a>#{'P' * 20}>#{MARKER}</a></top>")
  end

  # The gate: on develop this one fails with ["CANARY_LEAK_TEXT"]. The parse of
  # a three byte file read 413 bytes past the end of it.
  def test_truncated_file_does_not_leak_the_previous_parse
    prime_the_buffer
    seen = sax_file('<!D')
    assert(seen.none? { |v| v.include?(MARKER) },
           "content of an earlier parse came back: #{seen.inspect[0, 200]}")
  end

  # Guards rather than gates: how far the over-read gets depends on where the
  # buffer happens to sit, and these two pass on develop even though the reads
  # past read_end are there (8 of them for <!DOCTYPE).
  def test_truncated_doctype_does_not_leak
    prime_the_buffer
    seen = sax_file('<!DOCTYPE')
    assert(seen.none? { |v| v.include?(MARKER) },
           "content of an earlier parse came back: #{seen.inspect[0, 200]}")
  end

  def test_every_truncation_of_the_bang_forms
    prime_the_buffer
    ['<!DOCTYPE html>', '<!--comment-->', '<![CDATA[body]]>'].each do |whole|
      (1..whole.length).each do |n|
        seen = sax_file(whole[0, n])
        assert(seen.none? { |v| v.include?(MARKER) },
               "#{whole[0, n].inspect} leaked: #{seen.inspect[0, 120]}")
      end
    end
  end

  # Everything below must keep working.

  def test_complete_document_from_a_file
    assert_equal(%w[top a x a top], sax_file('<top><a>x</a></top>'))
  end

  def test_empty_file
    assert_equal([], sax_file(''))
  end

  def test_document_larger_than_the_initial_buffer
    body = "<top>#{'x' * 20_000}</top>"
    seen = sax_file(body)
    assert_equal('top', seen.first)
    assert_equal(20_000, seen.select { |v| v.start_with?('x') }.map(&:length).sum)
  end

  def test_doctype_and_comment_still_parse
    assert_equal(%w[top top], sax_file("<!DOCTYPE html>\n<!--c-->\n<top/>"))
  end

  def test_cdata_still_parses
    handler = Collect.new
    Tempfile.create(['cdata', '.xml']) do |f|
      f.write('<top><![CDATA[body]]></top>')
      f.flush
      File.open(f.path) { |io| Ox.sax_parse(handler, io) }
    end
    assert_equal(%w[top top], handler.seen)
  end

  # The string and StringIO paths never had the bug; they must be unchanged.
  def test_string_input_matches_file_input
    body = '<top><a>x</a></top>'
    from_string = Collect.new
    Ox.sax_parse(from_string, body)
    assert_equal(from_string.seen, sax_file(body))
  end
end
