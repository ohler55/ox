
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
    
    # Returns an array of Nodes or Strings that correspond to the locations
    # specified by the path parameter. The path parameter describes the path
    # to the return values which can be either nodes in the XML or
    # attributes. The path is a relative description. There are similarities
    # between the locate() method and XPath but locate does not follow the
    # same rules as XPath. The syntax is meant to be simpler and more Ruby
    # like.
    #
    # Like XPath the path delimiters are the slash (/) character. The path is
    # split on the delimiter and each element of the path then describes the
    # child of the current Element to traverse.
    #
    # Attributes are specified with an @ prefix.
    #
    # Each element name in the path can be followed by a bracket expression
    # that narrows the paths to traverse. Supported expressions are numbers
    # with a preceeding qualifier. Qualifiers are -, +, <, and >. The +
    # qualifier is the default. A - qualifier indicates the index begins at
    # the end of the children just like for Ruby Arrays. The < and >
    # qualifiers indicates all elements either less than or greater than
    # should be matched. Note that unlike XPath, the element index starts at 0
    # similar to Ruby be contrary to XPath.
    #
    # Element names can also be wildcard characters. A * indicates any
    # decendent should be followed. A ? indicates any single Element can
    # match the wildcard.
    #
    # Examples are:
    # * <code>element.locate("Family/Pete/*")</code> returns all children of the Pete Element.
    # * <code>element.locate("Family/?[1]")</code> returns the first element in the Family Element.
    # * <code>element.locate("Family/?[<3]")</code> returns the first 3 elements in the Family Element.
    # * <code>element.locate("Family/?/@age")</code> returns the arg attribute for each child in the Family Element.
    # * <code>element.locate("Family/*/@type")</code> returns the type attribute value for decendents of the Family.
    #
    # @param [String] path path to the Nodes to locate
    def locate(path)
      return [self] if path.nil?
      found = []
      pa = path.split('/')
      alocate(pa, found)
      found
    end
    
    # @param [Array] path array of steps in a path
    # @param [Array] found matching nodes
    def alocate(path, found)
      step = path[0]
      if step.start_with?('@') # attribute
        raise InvalidPath.new(path) unless 1 == path.size
        step = step[1..-1]
        sym_step = step.to_sym
        @attributes.each do |k,v|
          found << v if ('?' == step or k == step or k == sym_step)
        end
      else # element name
        if (i = step.index('[')).nil? # just name
          name = step
          qual = nil
        else
          name = step[0..i-1]
          raise InvalidPath.new(path) unless step.end_with?(']')
          i += 1
          qual = step[i..i] # step[i] would be better but some rubies (jruby, ree, rbx) take that as a Fixnum.
          if '0' <= qual and qual <= '9'
            qual = '+'
          else
            i += 1
          end
          index = step[i..-2].to_i
        end
        if '?' == name or '*' == name
          match = nodes
        else
          match = @nodes.select { |e| e.is_a?(Element) and name == e.name }
        end
        unless qual.nil? or match.empty?
          case qual
          when '+'
            match = index < match.size ? [match[index]] : []
          when '-'
            match = index <= match.size ? [match[-index]] : []
          when '<'
            match = 0 < index ? match[0..index - 1] : []
          when '>'
            match = index <= match.size ? match[index + 1..-1] : []
          else
            raise InvalidPath.new(path)
          end
        end
        if (1 == path.size)
          match.each { |n| found << n }
        elsif '*' == name
          match.each { |n| n.alocate(path, found) if n.is_a?(Element) }
          match.each { |n| n.alocate(path[1..-1], found) if n.is_a?(Element) }
        else
          match.each { |n| n.alocate(path[1..-1], found) if n.is_a?(Element) }
        end
      end
    end

  end # Element
end # Ox
