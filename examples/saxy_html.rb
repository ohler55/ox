#!/usr/bin/env ruby

# This example demonstrates the use of the Ox.sax_html parser and the
# Ox.Builder. The parser is used to parse and HTML file and add a
# `class="ppp"` to each '<p>' element start.
#
# The approach taken is to build while parsing. An HTML parse is started and a
# builder call is made on each parser callback. If the element is a 'p' then
# the class attribute is added. All others remain the same.

# Use the current repo if run from the examples directory.
ox_dir = File.dirname(File.dirname(File.expand_path(__FILE__)))
$LOAD_PATH << File.join(ox_dir, 'ext')
$LOAD_PATH << File.join(ox_dir, 'lib')

require 'ox'

# First create a handler for the SAX callbacks. The class instances include a
# builder that builds as parsing takes place.
class Saxy < Ox::Sax
  VOID_ELEMENTS = [:area, :base, :br, :col, :embed, :hr, :img, :input, :link, :meta, :param, :source, :track, :wbr]

  def initialize
    super
    # The build is created with an indentation of 2 but that can be changed to
    # the desired indentation.
    @builder = Ox::Builder.new(indent: 2)
    # element_name and attributes are used for deferred writing of the element
    # start.
    @element_name = nil
    @attrs = {}
  end

  def to_s
    @builder.to_s
  end

  # The builder creates element starts with attributes but the parser uses a
  # seprate call for attributes and element starts. To deal with the
  # difference keep track of the start name and attributes as they are
  # added. When another callback other than attributes is called write any
  # pending element start.
  def push_element
    return if @element_name.nil?

    # Add the class attribute if the element is a <p> element.
    @attrs[:class] = 'ppp' if :p == @element_name

    # Check @void_elements to determine how the element start would be
    # written. HTML includes void elements that are self closing so those
    # should be handled correctly.
    if VOID_ELEMENTS.include?(@element_name)
      @builder.void_element(@element_name, @attrs)
    else
      @builder.element(@element_name, @attrs)
    end
    # Reset the element name.
    @element_name = nil
    @attrs = {}
  end

  def start_element(name)
    push_element
    @element_name = name
  end

  def attr(name, value)
    @attrs[name] = value
  end

  def doctype(value)
    push_element
    @builder.doctype(value)
  end

  def comment(value)
    push_element
    @builder.comment(value)
  end

  def text(value)
    push_element
    @builder.text(value)
  end

  def end_element(name)
    push_element
    @builder.pop unless VOID_ELEMENTS.include?(name)
  end

  # Just in case there is a parse error this will display the error along with
  # where the error occurred in the XML file.
  def error(message, line, column)
    puts "*-*-* error at #{line}:#{column}: #{message}"
  end
end

# Load the XML file. The Ox.sax_html also handles IO objects.
xml = File.read('saxy.html')
# Create an instance of the handler.
handler = Saxy.new

Ox.sax_html(handler, xml)

# For debugging uncomment these lines.
# puts "******************** original *************************\n#{xml}"
# puts "******************** modifified ***********************\n#{handler.to_s}"

# For benchmarks these lines should be repeated to parse and to generate a
# modified XML string.
# handler = Saxy.new()
# Ox.sax_html(handler, xml)
# handler.to_s
