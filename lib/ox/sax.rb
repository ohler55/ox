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
  #    def start_element(name)
  #      @element_names << name
  #    end
  #  end
  #
  #  any = MySax.new()
  #  File.open('any.xml', 'r') do |f|
  #    Ox.sax_parse(any, f)
  #  end
  #
  # To make the desired methods active while parsing the desired method should
  # be made public in the subclasses. If the methods remain private they will
  # not be called during parsing. The 'name' argument in the callback methods
  # will be a Symbol. The 'str' arguments will be a String. The 'value'
  # arguments will be Ox::Sax::Value objects. Since both the text() and the
  # value() methods are called for the same element in the XML document the the
  # text() method is ignored if the value() method is defined or public. The
  # same is true for attr() and attr_value(). When all attribtues have been read
  # the attr_done() callback will be invoked.
  #
  #    def instruct(target); end
  #    def end_instruct(target); end
  #    def attr(name, str); end
  #    def attr_value(name, value); end
  #    def attrs_done(); end
  #    def doctype(str); end
  #    def comment(str); end
  #    def cdata(str); end
  #    def text(str); end
  #    def value(value); end
  #    def start_element(name); end
  #    def end_element(name); end
  #
  # Initializing @line in the initializer will cause that variable to be updated
  # before each callback with the XML line number. The same is true for the
  # @column but it will be updated with the column in the XML file that is the
  # start of the element or node just read.
  class Sax
    # Create a new instance of the Sax handler class.
    def initialize()
      #@line = nil
      #@column = nil
    end

    # To make the desired methods active while parsing the desired method
    # should be made public in the subclasses. If the methods remain private
    # they will not be called during parsing.
    private

    def instruct(target)
    end

    def end_instruct(target)
    end

    def attr(name, str)
    end

    def attr_value(name, value)
    end

    def attrs_done()
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
