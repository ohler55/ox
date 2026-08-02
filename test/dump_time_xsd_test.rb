#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: dump_time_xsd() dereferencing localtime()'s return value.
#
# localtime() returns NULL for a time it can not represent and dump_time_xsd()
# read through it without checking, so Ox.dump crashed with SIGSEGV. The range
# it rejects is platform specific, which is why both ends are covered here:
#
#   * the Microsoft CRT rejects every time before the epoch, so on Windows
#     Ox.dump(Time.at(-1), xsd_date: true) was enough
#   * glibc accepts those but rejects years too large for tm_year, so on Linux
#     it took something like Time.at(2**62)
#
# dump_time_xsd() no longer calls localtime() at all -- the offset comes from
# Time#utc_offset and the date arithmetic from time_conv.h -- so there is no
# NULL left to dereference and no range to fall outside of. These stay as the
# guard on that: every one of them crashed the process before, and they are
# still the widest inputs the function takes.
#
# The offset that gets written is asserted in xsd_time_test.rb, which is where
# the rest of the timezone handling lives. The assertions here accept either
# the local or the UTC rendering because a zone whose offset is not a whole
# number of minutes is written as UTC on purpose.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpTimeXsdTest < ::Test::Unit::TestCase
  XSD = /\A(-?\d{4,})-(\d\d)-(\d\d)T(\d\d):(\d\d):(\d\d)\.(\d{6})([-+]\d\d:\d\d)\z/

  def setup
    @opts = Ox.default_options
    Ox.default_options = {mode: :object, xsd_date: true}
  end

  def teardown
    Ox.default_options = @opts
  end

  def body(t)
    Ox.dump(t).strip[%r{<t>(.*)</t>}, 1]
  end

  def rendering(t)
    format('%04d-%02d-%02dT%02d:%02d:%02d.%06d', t.year, t.month, t.day, t.hour, t.min, t.sec, t.usec)
  end

  # The wall clock has to be the instant it was given, read either as local or
  # as UTC, and reading the document back has to land on that same instant.
  def assert_dumps_instant(sec)
    got = body(Time.at(sec))
    assert_match(XSD, got, "sec=#{sec}")
    t = Time.at(sec)
    assert_include([rendering(t.getlocal), rendering(t.getutc)], got[0, got.index(/[-+]\d\d:\d\d\z/)], "sec=#{sec}")
    assert_equal(sec, Ox.load("<t>#{got}</t>").to_i, "sec=#{sec}")
  end

  # The Windows end of the range. Before the fix these crashed the process
  # there, and CI runs Windows, so this is the case that guards it.
  def test_before_the_epoch_dumps
    [-1, -14_215_340, -2_208_988_800, -(2**40)].each { |sec| assert_dumps_instant(sec) }
  end

  # The glibc end. Both signs, since the day is split differently for a
  # negative remainder.
  def test_year_too_large_for_localtime_dumps
    assert_match(/\A\d{6,}-\d\d-\d\dT/, body(Time.at(2**62)))
    assert_match(/\A-\d{6,}-\d\d-\d\dT/, body(Time.at(-(2**62))))
  end

  # 2**62 is outside every platform's localtime() range, so the arithmetic here
  # is entirely ox's and has to agree with Ruby about which instant it is. A
  # date that far back can carry a local mean time offset -- Tokyo's was
  # +09:18:59 -- which is why assert_dumps_instant accepts UTC as well.
  def test_extreme_times_match_ruby
    [2**62, -(2**62), 2**62 - 12_345, -(2**62) + 98_765].each { |sec| assert_dumps_instant(sec) }
  end

  # A year outside four digits makes the line longer than the 33 bytes the old
  # code reserved, so the reservation is now taken from the formatted length.
  # Dumping many of them in one document exercises the grow() path.
  def test_long_years_do_not_truncate
    times = (0..200).map { |i| Time.at(2**55 + i * 1_000_000_007) }
    xml = Ox.dump(times)
    assert_equal(201, xml.scan('<t>').size)
    xml.scan(%r{<t>([^<]*)</t>}).flatten.each { |s| assert_match(/\A\d{10}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{6}[-+]\d\d:\d\d\z/, s) }
  end

  # Times localtime() can represent keep the local wall clock they always had,
  # including across a DST transition.
  def test_representable_times_keep_the_local_wall_clock
    [0, 1, 951_782_400, 1_483_660_687, 1_616_893_200, 1_636_264_800, 2**31].each do |sec|
      t = Time.at(sec)
      assert_equal(rendering(t.getlocal), body(t)[0, 26], "sec=#{sec}")
    end
  end

  def test_sub_second_digits_are_kept
    assert_equal('123456', body(Time.at(1_483_660_687, 123_456.789))[20, 6])
  end
end
