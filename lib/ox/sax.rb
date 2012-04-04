module Ox
  # A SAX style parse handler. The Ox::Sax handler class should be subclasses
  # and then used with the Ox.sax_parse() method. The Sax methods will then be
  # called as the file is parsed. This is best suited for very large files or
  # IO streams.<p/>
  # @example
  # 
  #  require 'ox'
  #
  #  class MySax < ::Ox::Sax
  #    def initialize()
  #      @element_name = []
  #    end
  #
  #    def start_element(name, attrs)
  #      @element_names << name
  #    end
  #  end
  #
  #  any = MySax.new()
  #  File.open('any.xml', 'r') do |f|
  #    Xml.sax_parse(any, f)
  #  end
  #
  # To make the desired methods active while parsing the desired method should
  # be made public in the subclasses. If the methods remain private they will
  # not be called during parsing. The 'name' argument in the callback methods
  # will be a Symbol. The 'str' arguments will be a String. The 'value'
  # arguments will be Ox::Sax::Value objects. Since both the text() and the
  # value() methods are called for the same element in the XML document the
  # the text() method is ignored if the value() method is defined or
  # public. The same is true for attr() and attr_value().
  #
  #    def instruct(target); end
  #    def attr(name, str); end
  #    def attr_value(name, value); end
  #    def doctype(str); end
  #    def comment(str); end
  #    def cdata(str); end
  #    def text(str); end
  #    def value(value); end
  #    def start_element(name); end
  #    def end_element(name); end
  #
  class Sax
    # Create a new instance of the Sax handler class.
    def initialize()
    end

    # To make the desired methods active while parsing the desired method
    # should be made public in the subclasses. If the methods remain private
    # they will not be called during parsing.
    private

    def instruct(target)
    end

    def attr(name, str)
    end

    def attr_value(name, value)
    end

    def doctype(str)
    end

    def comment(str)
    end

    def cdata(str)
    end

    def text(str)
    end

    def value(value)
    end

    def start_element(name)
    end

    def end_element(name)
    end
    
    def error(message, line, column)
    end
    
  end # Sax
end # Ox
