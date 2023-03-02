#!/usr/bin/env ruby

$: << '.'
$: << '..'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0 && (i = ARGV.index('-I'))
  x, path = ARGV.slice!(i, 2)
  $: << path
end

require 'optparse'
require 'ox'

it = 5000

opts = OptionParser.new
opts.on('-i', '--iterations [Int]', Integer, 'iterations')    { |i| it = i }
opts.on('-h', '--help', 'Show this display')                  { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

module Test
  module Ox
    class Wrap
      attr_accessor :values

      def initialize(v=[])
        @values = v
      end
    end
  end
end

data = { 
  :Boolean => ::Test::Ox::Wrap.new(),
  :Fixnum => ::Test::Ox::Wrap.new(),
  :Float => ::Test::Ox::Wrap.new(),
  :String => ::Test::Ox::Wrap.new(),
  :Symbol => ::Test::Ox::Wrap.new(),
  :Time => ::Test::Ox::Wrap.new(), 
  :Array => ::Test::Ox::Wrap.new(), 
  :Hash => ::Test::Ox::Wrap.new(),
  :Range => ::Test::Ox::Wrap.new(),
  :Regexp => ::Test::Ox::Wrap.new(),
  :Bignum => ::Test::Ox::Wrap.new(),
  :Complex => ::Test::Ox::Wrap.new(),
  :Rational => ::Test::Ox::Wrap.new(),
  :Struct => ::Test::Ox::Wrap.new(),
  :Class => ::Test::Ox::Wrap.new(),
  :Object => ::Test::Ox::Wrap.new(),
}

s = Struct.new('Zoo', :x, :y, :z)

(1..200).each do |i|
  data[:Boolean].values << (0 == (i % 2))
  data[:Fixnum].values << ((i - 10) * 101)
  data[:Float].values << ((i.to_f - 10.7) * 30.5)
  data[:String].values << "String #{i}"
  data[:Symbol].values << "Symbol#{i}".to_sym
  data[:Time].values << Time.now + i
  data[:Array].values << []
  data[:Hash].values << { }
  data[:Range].values << (0..7)
  data[:Regexp].values << /^[0-9]/
  data[:Bignum].values << (7 ** 55)
  data[:Complex].values << Complex(1, 2)
  data[:Rational].values << Rational(1, 3)
  data[:Struct].values << s.new(1, 3, 5)
  data[:Class].values << ::Test::Ox::Wrap
  data[:Object].values << ::Test::Ox::Wrap.new(i)
end

puts '           load                        dump'
puts 'type       Ox      Marshal  ratio      Ox      Marshal  ratio'
data.each do |type, a|
  # xml = Ox.dump(a, :indent => -1, :xsd_date => true)
  xml = Ox.dump(a, :indent => -1)
  # pp a
  # puts xml
  start = Time.now
  (1..it).each do
    obj = Ox.load(xml, :mode => :object)
    # pp obj
  end
  ox_load_time = Time.now - start
  
  m = Marshal.dump(a)
  start = Time.now
  (1..it).each do
    obj = Marshal.load(m)
  end
  mars_load_time = Time.now - start

  obj = Ox.load(xml, :mode => :object)
  start = Time.now
  (1..it).each do
    xml = Ox.dump(a, :indent => -1)
  end
  ox_dump_time = Time.now - start

  start = Time.now
  (1..it).each do
    m = Marshal.dump(a)
  end
  mars_dump_time = Time.now - start

  puts '%8s  %6.3f  %6.3f    %0.1f       %6.3f  %6.3f    %0.1f' % [type.to_s,
                                                                   ox_load_time, mars_load_time, mars_load_time / ox_load_time,
                                                                   ox_dump_time, mars_dump_time, mars_dump_time / ox_dump_time]
end
