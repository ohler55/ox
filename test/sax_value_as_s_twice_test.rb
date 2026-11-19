#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Sax::Value#as_s called more than once on the same value.
#
# as_s collapsed the special characters in place in the buffer it reads from,
# so a second call decoded the already decoded text again and &amp;lt; came
# back as < instead of &lt;.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class SaxValueAsSTwiceTest < ::Test::Unit::TestCase
  class Twice < ::Ox::Sax
    attr_reader :reads

    def initialize
      super
      @reads = []
    end

    def attr_value(_name, value)
      @reads << [value.as_s, value.as_s]
    end

    def value(value)
      @reads << [value.as_s, value.as_s]
    end
  end

  def parse(xml, opts)
    h = Twice.new
    Ox.sax_parse(h, StringIO.new(xml), opts)
    h.reads
  end

  def test_convert_special
    xml = '<top a="&amp;lt;b&amp;gt;">&amp;lt;script&amp;gt;</top>'

    assert_equal([['&lt;b&gt;', '&lt;b&gt;'], ['&lt;script&gt;', '&lt;script&gt;']],
                 parse(xml, convert_special: true))
  end

  def test_convert_special_char_ref
    xml = '<top>&amp;#60;x&amp;#62;</top>'

    assert_equal([['&#60;x&#62;', '&#60;x&#62;']], parse(xml, convert_special: true))
  end

  def test_skip_white
    xml = "<top> &amp;lt;a \r\n b&amp;gt; </top>"

    assert_equal([[' &lt;a b&gt; ', ' &lt;a b&gt; ']], parse(xml, convert_special: true, skip: :skip_white))
  end
end
