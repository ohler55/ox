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

def oxpoo(cnt = 100000)
  xml = "<?xml version=\"1.0\"?>\n<a>\n  <m>inc</m>\n  <i>1</i>\n</a>\n" 
  cnt.times do |i|
    obj = Ox.load(xml, :mode => :object)
    #puts "#{obj} (#{obj.class})"
    raise "decode ##{i} not equal; #{obj.inspect} != '#{[:inc, 1] }'" unless [:inc, 1] == obj
  end
end

oxpoo()
