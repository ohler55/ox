#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the xsd dateTime conversion, both directions.
#
# Load. parse_xsd_time() read the offset into cargs[7] and cargs[8], never
# looked at them, and called mktime(), which reads a struct tm as *local* time.
# So the offset the document carried was thrown away and every document was
# read as if it had been written on the reading machine. The struct tm was also
# never initialised, so mktime() read tm_isdst off the stack, and the Microsoft
# CRT returns -1 from mktime() for anything before the epoch. The fraction was
# written as microseconds and read back as nanoseconds, a factor of 1000.
#
# Dump. dump_time_xsd() took the offset from localtime()'s tm_gmtoff, which the
# Microsoft CRT does not have, so Windows wrote a local wall clock next to
# +00:00 and the document meant a different instant than it was given.
#
# Both sides now avoid the C library: the offset comes from Time#utc_offset and
# the date arithmetic is Hinnant's civil calendar in time_conv.h. That is one
# change rather than two because fixing the load alone would have broken the
# Windows round trip, which only worked because the two faults cancelled.
#
# Nothing here needs a particular process timezone -- the offsets are carried by
# the Time objects and the documents themselves -- except the two tests at the
# end that say so.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'time'
require 'ox'

class XsdTimeTest < ::Test::Unit::TestCase
  class Collect < ::Ox::Sax
    attr_reader :time

    def initialize
      @time = nil
    end

    def value(v)
      @time = v.as_time
    end
  end

  OFFSETS = ['+00:00', '+09:00', '-08:00', '+05:30', '-11:00', '+14:00', '-12:00'].freeze

  # Chosen for what they sit on: the epoch, a DST changeover in both directions,
  # the 32 bit time_t boundary, and dates before the epoch that the Microsoft
  # CRT's mktime() rejected.
  INSTANTS = [
    0, 1, 86_399, 1_483_628_287, 1_609_459_200, 1_615_766_400, 1_636_329_600,
    2_147_483_647, 2_147_483_648, -1, -14_182_940, -2_208_988_800, 4_102_444_800
  ].freeze

  def sax_time(xml)
    handler = Collect.new
    Ox.sax_parse(handler, xml)
    handler.time
  end

  def doc_for(sec, offset)
    Time.at(sec).getlocal(offset).strftime("%Y-%m-%dT%H:%M:%S.%6N#{offset}")
  end

  # The gate, and it does not need a timezone to bite: on develop 104 of these
  # 117 are wrong even with TZ=UTC, because the offset is discarded rather than
  # misapplied. Ruby's own parser is the reference.
  def test_load_honours_the_offset_in_the_document
    wrong = []
    INSTANTS.each do |sec|
      OFFSETS.each do |offset|
        doc = doc_for(sec, offset)
        want = Time.parse(doc).to_i
        got = Ox.parse_obj("<t>#{doc}</t>").to_i
        wrong << "#{doc} => #{got}, want #{want}" if got != want
      end
    end
    assert_equal([], wrong)
  end

  def test_the_sax_path_honours_it_too
    wrong = []
    INSTANTS.each do |sec|
      OFFSETS.each do |offset|
        doc = doc_for(sec, offset)
        want = Time.parse(doc).to_i
        got = sax_time("<t>#{doc}</t>").to_i
        wrong << "#{doc} => #{got}, want #{want}" if got != want
      end
    end
    assert_equal([], wrong)
  end

  # The same wall clock with different offsets is different instants. On develop
  # all seven of these are equal.
  def test_the_same_wall_clock_at_different_offsets_differs
    seen = OFFSETS.map { |o| Ox.parse_obj("<t>2017-01-05T23:58:07.000000#{o}</t>").to_i }
    assert_equal(OFFSETS.size, seen.uniq.size, seen.inspect)
  end

  def test_dump_writes_the_offset_the_time_carries
    OFFSETS.each do |offset|
      t = Time.at(1_483_628_287).getlocal(offset)
      xml = Ox.dump(t, xsd_date: true).strip
      assert_equal("<t>#{t.strftime("%Y-%m-%dT%H:%M:%S.%6N#{offset}")}</t>", xml, offset)
    end
  end

  def test_round_trip_is_exact
    drift = []
    INSTANTS.each do |sec|
      OFFSETS.each do |offset|
        t = Time.at(sec).getlocal(offset)
        back = Ox.parse_obj(Ox.dump(t, xsd_date: true))
        drift << "#{t.iso8601} => #{back.to_i}, want #{t.to_i}" if back.to_i != t.to_i
      end
    end
    assert_equal([], drift)
  end

  # mktime() on the Microsoft CRT returns -1 for every one of these, so they all
  # loaded as 1969-12-31T23:59:59Z. The arithmetic has no epoch floor.
  def test_before_the_epoch
    {
      '1969-07-20T20:17:40.000000+00:00' => -14_182_940,
      '1969-12-31T23:59:59.000000+00:00' => -1,
      '1900-01-01T00:00:00.000000+00:00' => -2_208_988_800,
      '1854-01-05T12:00:00.000000+00:00' => -3_660_206_400
    }.each do |doc, want|
      assert_equal(want, Ox.parse_obj("<t>#{doc}</t>").to_i, doc)
      assert_equal(want, sax_time("<t>#{doc}</t>").to_i, doc)
    end
  end

  def test_past_the_32_bit_boundary
    {
      '2038-01-19T03:14:07.000000+00:00' => 2_147_483_647,
      '2038-01-19T03:14:08.000000+00:00' => 2_147_483_648,
      '9999-12-31T23:59:59.000000+00:00' => 253_402_300_799
    }.each do |doc, want|
      assert_equal(want, Ox.parse_obj("<t>#{doc}</t>").to_i, doc)
      assert_equal(want, sax_time("<t>#{doc}</t>").to_i, doc)
    end
  end

  # The fraction is written as six digits and was read back as nanoseconds, so
  # half a second came out as half a millisecond. A document from elsewhere can
  # carry one to nine digits.
  def test_the_fraction_scales_to_its_digit_count
    {
      '.0' => 0, '.1' => 100_000_000, '.12' => 120_000_000, '.123' => 123_000_000,
      '.1234' => 123_400_000, '.123456' => 123_456_000, '.123456789' => 123_456_789
    }.each do |frac, want|
      doc = "2017-01-05T23:58:07#{frac}+00:00"
      assert_equal(want, Ox.parse_obj("<t>#{doc}</t>").nsec, doc)
      assert_equal(want, sax_time("<t>#{doc}</t>").nsec, doc)
    end
  end

  def test_sub_second_round_trip
    [0, 1_000, 123_456_000, 500_000_000, 999_999_000].each do |nsec|
      t = Time.at(1_483_628_287, nsec / 1000.0)
      assert_equal(nsec, Ox.parse_obj(Ox.dump(t, xsd_date: true)).nsec, nsec.to_s)
    end
  end

  # xsd writes the offset as hh:mm. Dublin was -00:25:21 until 1916, and writing
  # that as -00:25 would move the instant by 21 seconds, so UTC is written
  # instead. The instant is what has to survive.
  def test_an_offset_that_is_not_a_whole_minute_is_written_as_utc
    t = Time.at(-2_208_988_800).getlocal(-1521)
    xml = Ox.dump(t, xsd_date: true).strip
    assert_equal('<t>1900-01-01T00:00:00.000000+00:00</t>', xml)
    assert_equal(t.to_i, Ox.parse_obj(xml).to_i)
  end

  def test_a_malformed_time_still_falls_through_to_ruby
    assert_raise(ArgumentError) { Ox.parse_obj('<t>not a time</t>') }
  end

  # Everything above is timezone independent by construction. These two are the
  # ones that were not covered before, and they are the reason a UTC only CI
  # never saw any of this.

  def with_tz(tz)
    was = ENV['TZ']
    ENV['TZ'] = tz
    yield
  ensure
    ENV['TZ'] = was
  end

  # POSIX TZ strings rather than zone names so that the Microsoft CRT
  # understands them too.
  ZONES = ['UTC0', 'JST-9', 'PST8PDT,M3.2.0,M11.1.0', 'EST5EDT,M3.2.0,M11.1.0'].freeze

  def test_a_document_reads_the_same_in_every_timezone
    doc = '2017-01-05T23:58:07.000000+09:00'
    seen = ZONES.map { |tz| with_tz(tz) { Ox.parse_obj("<t>#{doc}</t>").to_i } }
    assert_equal([Time.parse(doc).to_i] * ZONES.size, seen)
  end

  # Written in one zone, read in another. This is what the two faults were
  # hiding from each other: it only worked when the zones matched.
  def test_written_in_one_zone_and_read_in_another
    written = ZONES.map { |tz| with_tz(tz) { Ox.dump(Time.at(1_483_628_287), xsd_date: true) } }
    ZONES.each do |tz|
      with_tz(tz) do
        written.each do |xml|
          assert_equal(1_483_628_287, Ox.parse_obj(xml).to_i, "#{xml.strip} read under #{tz}")
        end
      end
    end
  end
end
