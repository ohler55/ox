require 'ox'

doc = Ox::Document.new(:version => '1.0')

top = Ox::Element.new('top')
top[:name] = 'sample'
doc << top

mid = Ox::Element.new('middle')
mid[:name] = 'second'
top << mid

bot = Ox::Element.new('bottom')
bot[:name] = 'third'
mid << bot

xml = Ox.dump(doc)
puts xml

doc2 = Ox.parse(xml)
puts "Same? #{doc == doc2}"
