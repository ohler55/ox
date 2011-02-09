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
require 'ox'

ox_only = false
it = 1000

opts = OptionParser.new
opts.on("-x", "ox only")                                      { ox_only = true }
opts.on("-i", "--iterations [Int]", Integer, "iterations")    { |i| it = i }
opts.on("-h", "--help", "Show this display")                  { puts opts }
files = opts.parse(ARGV)

obj = nil

puts "to String and writing #{files}\n\n"

data = files.map do |f|
  ox_obj = Ox.parse_file(f)
  xml = File.read(f)
  no_obj = Nokogiri::XML::Document.parse(xml, nil, nil, Nokogiri::XML::ParseOptions::DEFAULT_XML | Nokogiri::XML::ParseOptions::NOBLANKS)
  [f, ox_obj, no_obj]
end

start = Time.now
data.each do |d|
  (1..it).each do
    xml = d[1].to_s
  end
end
ox_time = Time.now - start
puts "to String #{it} times with ox took #{ox_time} seconds."

#puts obj.to_s

=begin
start = Time.now
data.each do |d|
  (1..it).each do
    xml = d[1].to_s
  end
end
ox_time = Time.now - start
puts "to file #{it} times with ox took #{ox_time} seconds.\n\n"
=end
if !ox_only

  start = Time.now
  data.each do |d|
    (1..it).each do
      xml = d[2].to_s
    end
  end
  no_time = Time.now - start
  puts "to String #{it} times with nokogiri took #{no_time} seconds."
=begin
  start = Time.now
  data.each do |d|
    xml = d[1]
    (1..it).each do
      xml = d[2].to_s
    end
  end
  no_time = Time.now - start
  puts "to file #{it} times with nokogiri took #{no_time} seconds.\n\n"
=end
  puts "Ox is %0.1f faster than Nokogiri" % [no_time/ox_time]
end
