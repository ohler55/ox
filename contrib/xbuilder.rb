# Contributed by Notezen

require 'ox'
module Ox
  module XBuilder
    # args = attributes and/or children in any order, multiple appearance is possible
    # @overload build(name,attributes,children)
    #   @param [String] name name of the Element
    #   @param [Hash] attributes
    #   @param [String|Element|Array] children text, child element or array of elements
    def x(name, *args)
      n = Element.new(name)

      for arg in args
        case arg
        when Hash
          arg.each { |k, v| n[k.to_s] = v }
        when Array
          arg.each { |c| n << c if c}
        else
          n << arg if arg
        end
      end

      n
    end

    def x_if(condition, *args)
      x(*args) if condition
    end
  end
end
