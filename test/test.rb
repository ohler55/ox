#!/usr/bin/env ruby -wW1

$: << '.'
$: << 'lib'
$: << 'ext'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'optparse'
require 'pp'
require 'ox'

opts = OptionParser.new
opts.on("-v", "--verbose", "display parse information") { Ox.debug = Ox.debug + 1 }
opts.on("-h", "--help", "Show this display")            { puts opts }
files = opts.parse(ARGV)

module Park
  class Animal
    def initialize
      @type = nil
    end
  end

  class Zoo
    
    def self.create(n)
      z = self.new
      z.lion = n
      z.tiger = n * 2
      x
    end

    def initialize
      @lion = nil
      @tiger = nil
      @bear = nil
      @oh_my = nil
      @oh_pi = nil
      @big_guy = nil
      @tea_time = nil
      @minutes = nil
      @empty = nil
      @mixed = nil
      @clueless = nil
      @clues = nil
      @nest = nil
      @bb = "<base64>"
      @roar = (3..7)
    end

  end # Zoo
end # Park

files.each do |f|
  puts "parsing #{f}"
  obj = Ox.file_to_obj(f)
  puts "result:"
  pp obj
  #puts "***\n#{obj.instance_variable_get(:@raw)}***"
  obj.instance_variable_set(:@roar, (3..7))
  #Ox.to_file(obj, "foo.xml")
  s = Ox.dump(obj, :indent => -1)
  puts s
end
