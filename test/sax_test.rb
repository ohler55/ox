#!/usr/bin/env ruby -wW1
# encoding: UTF-8

$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  while (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'stringio'
require 'test/unit'
require 'optparse'
require 'ox'

opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
files = opts.parse(ARGV)

class MySax < ::Ox::Sax
  def initialize()
    @element_name = []
  end

  def instruct(target, attrs)
    puts "*** target #{target}, attrs: #{attrs}"
  end

  def start_element(name, attrs)
    puts "*** start #{name}, attrs: #{attrs}"
    @element_names << name
  end
end
  

class Func < ::Test::Unit::TestCase

  def test_sax_instruct
    ms = MySax.new()
    input = StringIO.new(%{<?xml?>})
    Ox.sax_parse(ms, input)
    puts ms
  end

  def xtest_sax_basic
    ms = MySax.new()
    input = StringIO.new(%{<top/>})
    Ox.sax_parse(ms, input)
    puts ms
  end

  def xtest_sax_nested
    ms = MySax.new()
    xml = %{<?xml version="1.0"?>
<top>
  <child>
    <grandchild/>
  <child/>
  <child>
    <grandchild/>
    <grandchild/>
  <child/>
</top>
}
    input = StringIO.new(xml)
    Ox.sax_parse(ms, input)
    puts ms
  end

  def xtest_sax_io_pipe
    ms = MySax.new()
    input,w = IO.pipe
    w << %{<top/>}
    w.close
    Ox.sax_parse(ms, input)
    puts ms
  end

  def xtest_sax_io_file
    ms = MySax.new()
    input = IO.open(IO.sysopen('basic.xml'))
    Ox.sax_parse(ms, input)
    input.close
    puts ms
  end

end

