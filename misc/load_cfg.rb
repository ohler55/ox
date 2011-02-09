#!/usr/bin/env ruby -wW1

$: << '.'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'optparse'
require 'pp'
require 'nokogiri'
require 'pod/xml'
require 'pod/cfg'
require 'ox'

cfg_dir = nil

opts = OptionParser.new
opts.on("-d", "--dir [String]", String, "dir for cfg")  { |d| cfg_dir = d }
opts.on("-v", "--verbose", "display parse information") { Ox.debug = Ox.debug + 1 }
opts.on("-h", "--help", "Show this display")            { puts opts }
files = opts.parse(ARGV)

cfg_dir = File.expand_path(cfg_dir)

start = Time.now
Dir.foreach(cfg_dir) do |d|
  next if '.' == d[0]
  d = File.join(cfg_dir, d)
  Dir.foreach(d) do |f|
    next if !f.end_with?('.xml')
    #xml = File.read(File.join(d, f))
    obj = Ox.file_to_obj(File.join(d, f))
  end
end
ox_time = Time.now - start
puts "Loading cfg with Ox took #{ox_time} seconds."

start = Time.now
Dir.foreach(cfg_dir) do |d|
  next if '.' == d[0]
  d = File.join(cfg_dir, d)
  Dir.foreach(d) do |f|
    next if !f.end_with?('.xml')
    xml = File.read(File.join(d, f))
    obj = ::Pod::Xml.to_obj(xml)
  end
end
no_time = Time.now - start
puts "Loading cfg with Nokogiri took #{no_time} seconds."

puts "Ox is %0.1f faster than Nokogiri" % [no_time/ox_time]
