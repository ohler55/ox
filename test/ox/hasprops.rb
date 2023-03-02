module Test
  module Ox
    module HasProps
      def add_prop(key, value)
        @props = { } unless instance_variable_defined?(:@props)
        @props[key] = value
      end

      def props
        @props = { } unless instance_variable_defined?(:@props)
        @props
      end
      
    end # HashProps
  end # Ox
end # Test
