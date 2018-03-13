#!/usr/bin/env ruby

# This example demonstrates the use of the Ox.sax_parse method. An XML string
# is parsed and a list of element names if built.

# Use the current repo if run from the examples directory.
ox_dir = File.dirname(File.dirname(File.expand_path(__FILE__)))
$LOAD_PATH << File.join(ox_dir, 'ext')
$LOAD_PATH << File.join(ox_dir, 'lib')

require 'ox'

# First create a handler for the SAX callbacks. A Hash is used to collect the
# element names. This is a quick way to make sure the collected names are
# unique. Only the start_element is implemented as that is all that is needed
# to collect names. There is no need to inherit from Ox::Sax but tht class
# includes the private version of all the methods that can be made publis.
class Saxy
  def initialize
    #super
    @names = {}
  end

  def names
    @names.keys
  end

  def start_element(name)
    @names[name] = nil
  end

end

# The XML can be a string or a IO instance.
xml = %{
<?xml version="1.0"?>
<Park.Animal>
  <type>mutant</type>
  <friends type="Hash">
    <i>5</i>
    <Park.Animal>
      <type>dog</type>
    </Park.Animal>
  </friends>
</Park.Animal>
}
# Create an instance of the handler. 
handler = Saxy.new()

Ox.sax_html(handler, xml)

puts "element names: #{handler.names}"
