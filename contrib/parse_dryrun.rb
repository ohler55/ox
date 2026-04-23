#!/usr/bin/env ruby
# frozen_string_literal: true

# Parse the given file using either Ox.parse or Ox.sax_parse

require 'ox'

class MyHandler < Ox::Sax
  def initialize
    @line = nil
    @column = nil
  end

  def error(message, line, column); end
  def start_element(name); end
  def attr(name, str); end
  def attr_value(name, value); end
  def end_element(name); end
  def text(str); end
  def value(value); end
  def instruct(target); end
  def doctype(value); end
  def comment(value); end
  def cdata(value); end
end

@sax_mode = (ARGV.first == '--sax' && ARGV.shift)
def sax_mode?
  @sax_mode
end

# Support - filename to use stdin
def open(&block)
  if ARGV.first == '-'
    block.call $stdin
  else
    File.open ARGV.first, 'r', &block
  end
end

open do |file|
  begin
    if sax_mode?
      Ox.sax_parse MyHandler.new, file
    else
      Ox.parse file.read
    end
  rescue Ox::ParseError, EncodingError => e
    puts e.inspect
  end
end
