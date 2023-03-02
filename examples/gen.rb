#!/usr/bin/env ruby

# This example demonstrates loading an XML modifying it and then dumping it.

# Use the current repo if run from the examples directory.
ox_dir = File.dirname(File.dirname(File.expand_path(__FILE__)))
$LOAD_PATH << File.join(ox_dir, 'ext')
$LOAD_PATH << File.join(ox_dir, 'lib')

require 'ox'

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

# Load the XML into a set of Ox::Nodes.
doc = Ox.load(xml, mode: :generic)

# Once an Ox::Document is loaded it can be inspected and modified. A Doc has a
# root. Calling doc.root will give a node that is the root of the XML which is
# the Park.Animal element.
root = doc.root
puts "root element name: #{root.name}"

# The Ox::Element.locate method can be used similar to XPath. It does not have
# all the features of XPath but it does help dig into an XML. Look for any
# descendent of the root that has a type attribute and return those attribute
# values.
puts "descendent type attribute value: #{root.locate('*/@type')}"

# Delete 'i' element by iterating over the root's nodes and look for one named
# friends. The locate method could also be used.
root.nodes.each { |n|
  n.nodes.delete_if { |child| child.name == 'i' } if n.name == 'friends'
}

# Lets take a look at the changes by dumping the doc.
xml2 = Ox.dump(doc)
puts "modified XML: #{xml2}"
