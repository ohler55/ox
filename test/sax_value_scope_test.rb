#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: an Ox::Sax::Value used outside of its callback.
#
# The Value passed to value() and attr_value() pointed at the SAX drive, which
# lives in the C stack frame of Ox.sax_parse. A handler that kept the Value and
# read it after the parse read through that dead frame and crashed, and one
# that read it from a later callback got whatever the parser was reading then.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class SaxValueScopeTest < ::Test::Unit::TestCase
  class Keep < ::Ox::Sax
    attr_reader :values, :texts, :late

    def initialize
      super
      @values = []
      @texts = []
      @late = []
    end

    def attr_value(_name, value)
      @values << value
      @texts << value.as_s
    end

    def value(value)
      @values << value
      @texts << value.as_s
    end

    def end_element(_name)
      @values.each do |v|
        @late << v.as_s
      rescue Ox::ArgError => e
        @late << e.class
      end
    end
  end

  def parse
    h = Keep.new
    Ox.sax_parse(h, StringIO.new('<top a="one"><child>two</child></top>'))
    h
  end

  def test_in_callback
    assert_equal(['one', 'two'], parse.texts)
  end

  def test_later_callback
    assert_equal([Ox::ArgError], parse.late.uniq)
  end

  def test_after_parse
    h = parse
    # Reuse the stack the parse ran on before reading the kept values.
    deep = ->(n) { n.zero? ? 0 : deep.call(n - 1) + 1 }
    deep.call(200)
    h.values.each do |v|
      [:as_s, :as_sym, :as_i, :as_f, :as_time, :as_bool, :empty?].each do |m|
        assert_raise(Ox::ArgError) { v.send(m) }
      end
    end
  end
end
