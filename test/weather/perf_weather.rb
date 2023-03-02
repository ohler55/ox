#!/usr/bin/env ruby -wW1

# This example uses SAX and DOM approaches to parsing XML files. It is also
# used to compare the differences in performance and code structure for
# multiple XML parsers.
#
# Weather files taken from the British of Columbia are used as a source for
# capturing the high temperatures over some period of time. The files are
# loaded into a spreadsheet and then dumped as XML. The high temperatures for
# each time is stored in a Hash but no further action is taken on the data.

$: << File.dirname(__FILE__)
$: << File.join(File.dirname(__FILE__), '..')
$: << File.join(File.dirname(__FILE__), '../../lib')
$: << File.join(File.dirname(__FILE__), '../../ext')

require 'optparse'
require 'perf'

$filename = 'weather/HellsGate_2012_weather.xml'
$as_time = false
$iter = 10

opts = OptionParser.new
opts.on('-f', '--file [String]', String, 'filename')           { |f| $filename = f }
opts.on('-i', '--iterations [Int]', Integer, 'iterations')     { |i| $iter = i }
opts.on('-t', 'dates as time')                                 { $as_time = true }
opts.on('-h', '--help', 'Show this display')                   { puts opts; Process.exit!(0) }
rest = opts.parse(ARGV)

perf = Perf.new()

$sax = nil

begin
  require 'weather/wsax'
  perf.add('Ox::Sax', 'sax_parse') { Weather::WSax.parse($filename, $as_time) }
rescue Exception => e
end

begin
  require 'weather/wox'
  perf.add('Ox', 'parse') { Weather::WOx.parse($filename, $as_time) }
rescue Exception => e
end

begin
  require 'weather/wnosax'
  perf.add('Nokogiri::XML::Sax', 'parse') { Weather::WNoSax.parse($filename, $as_time) }
rescue Exception => e
end

begin
  require 'weather/wno'
  perf.add('Nokogiri::XML::Document', 'parse') { Weather::WNo.parse($filename, $as_time) }
rescue Exception => e
end

begin
  require 'weather/wlxsax'
  perf.add('LibXML::XML::SaxParser', 'parse') { Weather::WLxSax.parse($filename, $as_time) }
rescue Exception => e
end

begin
  require 'weather/wlx'
  perf.add('LibXML::XML::Document', 'parse') { Weather::WLx.parse($filename, $as_time) }
rescue Exception => e
end

perf.run($iter)
