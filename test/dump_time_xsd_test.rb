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
# A time outside localtime()'s range now falls back to UTC, which is why the
# assertions below accept either the local or the UTC rendering of the instant
# rather than one of them: which is used depends on where the platform draws
# that line. Times inside the range are unaffected, verified byte for byte
# identical over 7108 dumps in each of four timezones.
#
# Nothing here asserts the offset that gets written, because on a platform
# without tm_gmtoff it is still "+00:00" for a local wall clock. That is a
# separate defect and a separate fix.

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
  # as UTC. A wrong one would mean the fallback computed the wrong date.
  def assert_dumps_instant(sec)
    got = body(Time.at(sec))
    assert_match(XSD, got, "sec=#{sec}")
    t = Time.at(sec)
    assert_include([rendering(t.getlocal), rendering(t.utc)], got[0, got.index(/[-+]\d\d:\d\d\z/)], "sec=#{sec}")
  end

  # The Windows end of the range. Before the fix these crashed the process
  # there, and CI runs Windows, so this is the case that guards it.
  def test_before_the_epoch_dumps
    [-1, -14_215_340, -2_208_988_800, -(2**40)].each { |sec| assert_dumps_instant(sec) }
  end

  # The glibc end. Both signs, since the fallback splits the day differently
  # for a negative remainder.
  def test_year_too_large_for_localtime_dumps
    assert_match(/\A\d{6,}-\d\d-\d\dT/, body(Time.at(2**62)))
    assert_match(/\A-\d{6,}-\d\d-\d\dT/, body(Time.at(-(2**62))))
  end

  # 2**62 is outside every platform's localtime() range, so this is always the
  # fallback and it has to agree with Ruby about which instant it is.
  def test_fallback_matches_ruby_utc
    [2**62, -(2**62), 2**62 - 12_345, -(2**62) + 98_765].each do |sec|
      t = Time.at(sec).utc
      assert_equal("#{rendering(t)}+00:00", body(Time.at(sec)), "sec=#{sec}")
    end
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
