
module Ox

  # A generic class that is used only for storing attributes. It is the base
  # Class for auto-generated classes in the storage system. Instance variables
  # are added using the instance_variable_set() method. All instance variables
  # can be accessed using the variable name (without the @ prefix). No setters
  # are provided as the Class is intended for reading only.
  class Bag

    # The initializer can take multiple arguments in the form of key values
    # where the key is the variable name and the value is the variable
    # value. This is intended for testing purposes only.
    # Example: Ox::Bag.new(:@x => 42, :@y => 57)
    def initialize(args={ })
      args.each do |k,v|
        self.instance_variable_set(k, v)
        m = k.to_s[1..-1].to_sym
        unless respond_to?(m)
          self.class.define_get(m, k)
        end
      end
    end

    # Replaces the Object.respond_to?() method to return true for any method
    # than matches instance variables.
    # [m] method symbol
    def respond_to?(m)
      at_m = ('@' + m.to_s).to_sym
      instance_variables.include?(at_m)
    end

    # Handles requests for variable values. Others cause an Exception to be
    # raised.
    # [m] method symbol
    def method_missing(m, *args, &block)
      raise ArgumentError.new("wrong number of arguments(#{args.size} for 0)") unless args.nil? or args.empty?
      at_m = ('@' + m.to_s).to_sym
      raise NoMethodError("undefined method", m) unless instance_variable_defined?(at_m)
      instance_variable_get(at_m)
    end

    # Replace eql?() with something more reasonable for this Class.
    # [other] Object to compare this one to
    def eql?(other)
      return false if (other.nil? or self.class != other.class)
      ova = other.instance_variables
      iv = instance_variables
      return false if ova.size != iv.size
      iv.each do |vid|
        return false if instance_variable_get(vid) != other.instance_variable_get(vid)
      end
      true
    end
    alias == eql?
    
  end # Bag

  # Define a new class based on the Ox::Bag class. This is used internally in
  # the Ox module and is available to service wrappers that receive XML
  # requests that include Objects of Classes not defined in the storage
  # process.
  # [classname] Class name or symbol that includes Module names
  def self.define_class(classname)
    classname = classname.to_s unless classname.is_a?(String)
    tokens = classname.split('::').map { |n| n.to_sym }
    raise "Invalid classname '#{classname}" if tokens.empty?
    m = Object
    tokens[0..-2].each do |sym|
      if m.const_defined?(sym)
        m = m.const_get(sym)
      else
        c = Module.new
        m.const_set(sym, c)
        m = c
      end
    end
    sym = tokens[-1]
    if m.const_defined?(sym)
      c = m.const_get(sym)
    else
      c = Class.new(Ox::Bag)
      m.const_set(sym, c)
    end
    c
  end

end # Ox
