
module Ox
  # An Element represents a element of an XML document. It has a name,
  # attributes, and sub-nodes.
  class Element < Node
    
    # Creates a new Element with the specified name.
    # @param [String] name name of the Element
    def initialize(name)
      super
      @attributes = nil
      @nodes = nil
    end
    alias name value
    
    # Returns the Element's nodes array. These are the sub-elements of this
    # Element.
    # @return [Array] all child Nodes.
    def nodes
      @nodes = [] if !instance_variable_defined?(:@nodes) or @nodes.nil?
      @nodes
    end

    # Appends a Node to the Element's nodes array.
    # @param [Node] node Node to append to the nodes array
    def <<(node)
      @nodes = [] if !instance_variable_defined?(:@nodes) or @nodes.nil?
      raise "argument to << must be a String or Ox::Node." unless node.is_a?(String) or node.is_a?(Node)
      @nodes << node
    end

    # Returns all the attributes of the Element as a Hash.
    # @return [Hash] all attributes and attribute values.
    def attributes
      @attributes = { } if !instance_variable_defined?(:@attributes) or @attributes.nil?
      @attributes
    end
    
    # Returns the value of an attribute.
    # @param [Symbol|String] attr attribute name or key to return the value for
    def [](attr)
      return nil unless instance_variable_defined?(:@attributes) and @attributes.is_a?(Hash)
      @attributes[attr] or (attr.is_a?(String) ? @attributes[attr.to_sym] : @attributes[attr.to_s])
    end

    # Adds or set an attribute of the Element.
    # @param [Symbol|String] attr attribute name or key
    # @param [Object] value value for the attribute
    def []=(attr, value)
      raise "argument to [] must be a Symbol or a String." unless attr.is_a?(Symbol) or attr.is_a?(String)
      @attributes = { } if !instance_variable_defined?(:@attributes) or @attributes.nil?
      @attributes[attr] = value.to_s
    end

    # Returns true if this Object and other are of the same type and have the
    # equivalent value and the equivalent elements otherwise false is returned.
    # @param [Object] other Object compare _self_ to.
    # @return [Boolean] true if both Objects are equivalent, otherwise false.
    def eql?(other)
      return false if (other.nil? or self.class != other.class)
      return false unless super(other)
      return false unless self.attributes == other.attributes
      return false unless self.nodes == other.nodes
      true
    end
    alias == eql?

  end # Element
end # Ox
