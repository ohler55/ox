module Test
  module Ox
    class Layer < Group
      attr_accessor :name

      def initialize(name)
        super()
        @name = name
      end

    end # Layer
  end # Ox
end # Test
