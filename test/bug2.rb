#!/usr/bin/env ruby -wW1

$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'ox'

def bug2()
  s = File.read('long.pdf')
  puts "size before: #{s.size}"
  
  # s = "Hello\x00\x00\x00there."
  xml = Ox.dump(s, mode: :object)
  #puts "xml size: #{xml.size}"
  s2 = Ox.load(xml, mode: :object)
  puts "size after: #{s2.size}"
  #puts s2
end

bug2()
