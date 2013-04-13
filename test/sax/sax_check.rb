#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../../lib")
$: << File.join(File.dirname(__FILE__), "../../ext")

require 'ox'

class QuietSax < Ox::Sax
  
  def initialize()
    @line = nil
    @column = nil
  end
  def start_element(name)
    #puts "Start #{name} @ #{@line}:#{@column}"
  end
  def end_element(name)
    #puts "End #{name} @ #{@line}:#{@column}"
  end
  def attr(name, value); end
  def instruct(target); end
  def end_instruct(target); end
  def doctype(value); end
  def comment(value); end
  def cdata(value); end
  def text(value); end
  def error(message, line, column)
    puts "Error: #{message} @ #{line}:#{column}"
  end
end

handler = QuietSax.new()
Ox.sax_parse(handler, ARGF, :smart => true)
