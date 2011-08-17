
module Ox
  # Coments represent XML comments in an XML document. A comment as value
  # attribute only.
  class Comment < Node
    # Creates a new Comment with the specified value.
    # @param value [String] string value for the comment
    def initialize(value)
      super
    end

  end # Comment
end # Ox
