#!/usr/bin/env ruby

# This example demonstrates the use of Ox.load using the :hash and
# :hash_no_attrs modes.

# Use the current repo if run from the examples directory.
ox_dir = File.dirname(File.dirname(File.expand_path(__FILE__)))
$LOAD_PATH << File.join(ox_dir, 'ext')
$LOAD_PATH << File.join(ox_dir, 'lib')

require 'ox'

# load or use this sample string.
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

doc = Ox.load(xml, mode: :hash)
puts "as hash with Symbol element names: #{doc}"

# Load the XML and convert to a Hash. By default element names are
# symbolized. By using the :symbolize_keys option and setting it to false the
# element names will be strings.
doc = Ox.load(xml, mode: :hash, symbolize_keys: false)
puts "as hash with String element names: #{doc}"

# The :hash_no_attrs mode leaves attributes out of the resulting Hash.
doc = Ox.load(xml, mode: :hash_no_attrs)
puts "as hash_no_attrs: #{doc}"
