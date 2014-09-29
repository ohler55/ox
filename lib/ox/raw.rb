
module Ox
  # Coments represent XML comments in an XML document. A comment as value
  # attribute only.
  class Raw < Node
    # Creates a new Raw element with the specified value.
    # @param value [String] string value for the comment
    def initialize(value)
      super
    end

  end # Raw
end # Ox
