require 'ox'

class Sample
  attr_accessor :a, :b, :c

  def initialize(a, b, c)
    @a = a
    @b = b
    @c = c
  end
end # File

# Create Object
obj = Sample.new(1, 'bee', ['x', :y, 7.0])
# Now dump the Object to an XML String.
xml = Ox.dump(obj)
# Convert the object back into a Sample Object.
obj2 = Ox.parse_obj(xml)
