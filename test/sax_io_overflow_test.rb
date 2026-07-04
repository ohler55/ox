#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: SAX IO callback buffer overflow.
#
# Ox.sax_parse accepts any object responding to read()/readpartial(). The IO
# callbacks in sax_buf.c must clamp the returned data to the number of bytes
# they requested; otherwise a misbehaving IO that returns more than asked for
# overflows the parser buffer (which starts on the C stack).

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'stringio'
require 'ox'

class SaxIoOverflowTest < ::Test::Unit::TestCase
  class NullSax < ::Ox::Sax
    def start_element(_n); end
    def end_element(_n); end
    def text(_t); end
    def attr(_n, _v); end
    def error(_m, _l, _c); end
  end

  # IO that ignores the requested size and returns far more than asked for.
  class OverIO
    def initialize(payload)
      @payload = payload
      @sent = false
    end

    def read(_max)
      return nil if @sent

      @sent = true
      @payload
    end

    def readpartial(_max)
      raise EOFError if @sent

      @sent = true
      @payload
    end
  end

  def test_over_long_read_does_not_overflow
    big = "<r>#{'A' * 200_000}</r>"
    assert_nothing_raised do
      Ox.sax_parse(NullSax.new, OverIO.new(big.dup))
    end
  end

  def test_well_behaved_stringio_still_parses
    big = "<r>#{'A' * 200_000}</r>"
    assert_nothing_raised do
      Ox.sax_parse(NullSax.new, StringIO.new(big.dup))
    end
  end
end
