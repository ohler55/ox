#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the thin time format losing every time before the epoch.
#
# dump_time_thin() emitted the seconds with `for (; 0 < sec; ...)`, which runs
# zero times for a negative count, so the whole integer part disappeared:
#
#   Ox.dump(Time.at(-14_215_340), mode: :object)  =>  <t>.000000000</t>
#   Ox.load(that)                                 =>  1970-01-01 00:00:00 UTC
#
# No crash and no warning, just 1970 for anything older. This is the default
# path, since xsd_date defaults to false.
#
# The loaders could not read a sign either, so both halves are fixed together.
# There are two copies of parse_double_time(), one in obj_load.c and one in
# sax_as.c, and both are covered here.
#
# tv_nsec is never negative, so a time before the epoch is a negative second
# count carrying a positive fraction: Time.at(-0.5) is (-1, 500_000_000). It is
# written as the magnitude with a sign, "-0.500000000", rather than as those two
# fields, which would read as -1.5.
#
# Covering that turned up a second range defect at the other end. Both loaders
# accumulated the seconds into a long, which is 32 bits on Windows, so a time
# past 2038-01-19 came back with its sign flipped. Same two functions, so it is
# fixed here as well.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class DumpTimeThinTest < ::Test::Unit::TestCase
  def setup
    @opts = Ox.default_options
    Ox.default_options = {mode: :object, xsd_date: false}
  end

  def teardown
    Ox.default_options = @opts
  end

  def body(t)
    Ox.dump(t).strip[%r{<t>(.*)</t>}, 1]
  end

  def test_before_the_epoch_keeps_the_seconds
    assert_equal('-14215340.000000000', body(Time.at(-14_215_340)))
    assert_equal('-2209021200.000000000', body(Time.at(-2_209_021_200)))
    assert_equal('-1.000000000', body(Time.at(-1)))
  end

  def test_before_the_epoch_round_trips
    [-1, -86_400, -14_215_340, -2_209_021_200, -(2**40), -(2**62)].each do |sec|
      assert_equal(sec, Ox.load(Ox.dump(Time.at(sec))).to_i, "sec=#{sec}")
    end
  end

  # The fraction has to end up on the right side of the decimal point, which is
  # what makes this more than a sign character.
  def test_negative_fractions_round_trip
    [[-1, 500_000], [-2, 250_000], [-1, 999_999], [-1, 1], [0, 1], [1, 500_000]].each do |sec, usec|
      t = Time.at(sec, usec)
      assert_equal(t.to_r, Ox.load(Ox.dump(t)).to_r, "Time.at(#{sec}, #{usec})")
    end
  end

  def test_negative_fraction_reads_as_a_decimal
    assert_equal('-0.500000000', body(Time.at(-1, 500_000)))
    assert_equal('-1.750000000', body(Time.at(-2, 250_000)))
  end

  # The seconds loop used to emit nothing for zero as well, leaving a bare
  # ".000000000". It loaded back as zero, so this was only a malformed number,
  # but it is a number now.
  def test_the_epoch_has_an_integer_part
    assert_equal('0.000000000', body(Time.at(0)))
    assert_equal(0, Ox.load(Ox.dump(Time.at(0))).to_i)
  end

  # What older versions wrote for the epoch still has to load.
  def test_a_missing_integer_part_still_loads
    assert_equal(0, Ox.load('<t>.000000000</t>').to_i)
  end

  def test_after_the_epoch_is_unchanged
    assert_equal('1483660687.000000000', body(Time.at(1_483_660_687)))
    assert_equal('0.000001000', body(Time.at(0, 1)))
    [1, 86_400, 951_782_400, 2**31, 2**40].each do |sec|
      assert_equal(sec, Ox.load(Ox.dump(Time.at(sec))).to_i, "sec=#{sec}")
    end
  end

  # The loaders accumulated the seconds into a long, which is 32 bits on
  # Windows, so anything past 2038-01-19 came back with the sign flipped and
  # anything before 1901-12-13 did the same. Only visible on an LLP64 platform;
  # long is 64 bits everywhere else ox is built.
  def test_seconds_outside_32_bits_do_not_wrap
    {
      2**31 => '2038-01-19',           # first second a 32 bit long can not hold
      4_102_444_800 => '2100-01-01',
      -2_208_988_800 => '1900-01-01',  # magnitude is also past 2**31
      -(2**40) => '-46-05-11'
    }.each do |sec, _label|
      assert_equal(sec, Ox.load(Ox.dump(Time.at(sec))).to_i, "sec=#{sec}")
      assert_equal(sec, Ox.load("<t>#{sec}.000000000</t>").to_i, "text sec=#{sec}")
      assert_equal(sec, sax_time("#{sec}.000000000").to_i, "sax sec=#{sec}")
    end
  end

  # sax_as.c has its own copy of parse_double_time().
  def sax_time(s)
    handler = Class.new(::Ox::Sax) do
      attr_reader :got

      def attr_value(_name, value)
        @got = value.as_time
      end
    end.new
    Ox.sax_parse(handler, StringIO.new(%(<top t="#{s}"/>)))
    handler.got
  end

  def test_sax_as_time_reads_before_the_epoch
    assert_equal(-14_215_340, sax_time('-14215340.000000000').to_i)
    assert_equal(Rational(-1, 2), sax_time('-0.500000000').to_r)
    assert_equal(1_483_660_687, sax_time('1483660687.000000000').to_i)
  end

  # A leading '-' must not swallow an xsd time with a negative year, which the
  # loader hands to parse_xsd_time() instead.
  def test_negative_year_xsd_time_is_not_taken_as_a_double
    assert_nothing_raised { Ox.load('<t>-0044-03-15T12:00:00.000000+00:00</t>') }
  end

  # Text the sign does not make into a number still falls through to Time.parse
  # and raises there, the same as it always did. Consuming the '-' must not turn
  # any of it into a silent zero.
  def test_text_that_is_not_a_number_still_raises
    ['-', '-abc', 'abc'].each do |s|
      assert_raise(ArgumentError, s) { Ox.load("<t>#{s}</t>") }
    end
  end
end
