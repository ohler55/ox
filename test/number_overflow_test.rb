#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: decimal text wrapping around when it has too many digits.
#
# Ox::Sax::Value#as_i and an object mode <i> element accumulated their digits
# into a long, and both copies of parse_double_time() did the same with the
# seconds of a thin format time, with nothing checking for overflow. Signed
# overflow is undefined, and in practice the value wrapped to whatever the
# document picked:
#
#   as_i("9223372036854775808")              =>  -9223372036854775808
#   as_i("18446744073709551617")             =>  1
#   Ox.load('<i>18446744073709551617</i>')   =>  1
#   as_time("18446744073709551617.5")        =>  1970-01-01 00:00:01.5
#
# The integers now come back as the Integer the text holds, and seconds past the
# largest time_t fall through to Time.parse, which raises on them.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class NumberOverflowTest < ::Test::Unit::TestCase
  BIG = [
    '9223372036854775808',
    '18446744073709551617',
    '-9223372036854775809',
    '-18446744073709551621',
    '123456789012345678901234567890'
  ].freeze

  def sax_value(meth, s)
    handler = Class.new(::Ox::Sax) do
      attr_reader :got

      def initialize(meth)
        super()
        @meth = meth
      end

      def attr_value(_name, value)
        @got = value.send(@meth)
      end
    end.new(meth)
    Ox.sax_parse(handler, StringIO.new(%(<top v="#{s}"/>)))
    handler.got
  end

  def test_sax_as_i_does_not_wrap
    BIG.each do |s|
      assert_equal(Integer(s), sax_value(:as_i, s), s)
    end
    assert_equal(18_446_744_073_709_551_617, sax_value(:as_i, '+18446744073709551617'))
  end

  def test_sax_as_i_long_limits
    assert_equal(9_223_372_036_854_775_807, sax_value(:as_i, '9223372036854775807'))
    assert_equal(-9_223_372_036_854_775_808, sax_value(:as_i, '-9223372036854775808'))
    assert_equal(7, sax_value(:as_i, '7'))
  end

  # The digits past the point where a long ran out are still checked.
  def test_sax_as_i_still_rejects_a_bad_digit_after_many
    assert_raise(Ox::ArgError) { sax_value(:as_i, '18446744073709551617x') }
  end

  def test_object_fixnum_does_not_wrap
    BIG.each do |s|
      assert_equal(Integer(s), Ox.load("<i>#{s}</i>", mode: :object), s)
    end
    assert_equal(-9_223_372_036_854_775_808, Ox.load('<i>-9223372036854775808</i>', mode: :object))
  end

  def test_object_fixnum_still_rejects_a_bad_digit_after_many
    assert_raise(Ox::ParseError) { Ox.load('<i>18446744073709551617x</i>', mode: :object) }
  end

  def test_time_seconds_past_time_t_do_not_wrap
    ['9223372036854775808.0', '18446744073709551617.5'].each do |s|
      assert_raise(ArgumentError, s) { Ox.load("<t>#{s}</t>", mode: :object) }
      assert_raise(ArgumentError, s) { sax_value(:as_time, s) }
    end
  end

  def test_time_seconds_at_the_time_t_limit_still_load
    max = (2**63) - 1
    assert_equal(max, Ox.load("<t>#{max}.000000000</t>", mode: :object).to_i)
    assert_equal(max, sax_value(:as_time, "#{max}.000000000").to_i)
  end
end
