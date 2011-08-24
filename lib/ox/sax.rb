module Ox
  # A SAX style parse handler. The Ox::Sax handler class should be subclasses
  # and then used with the Ox.sax_parse() method. The Sax methods will then be
  # called as the file is parsed. This is best suited for very large files or
  # IO streams.<p/>
  # @example
  # 
  #  require 'ox'
  #
  # 
  # To make the desired methods active while parsing the desired method should
  # be made public in the subclasses. If the methods remain private they will
  # not be called during parsing.
  #
  #    def instruct(target, attrs); end
  #    def doctype(value); end
  #    def comment(value); end
  #    def cdata(value); end
  #    def text(value); end
  #    def start_element(name, attrs); end
  #    def end_element(name); end

  class Sax
    # Create a new instance of the Sax handler class.
    def initialize()
    end

    # To make the desired methods active while parsing the desired method
    # should be made public in the subclasses. If the methods remain private
    # they will not be called during parsing.
    private

    def instruct(target, attrs)
    end

    def doctype(value)
    end

    def comment(value)
    end

    def cdata(value)
    end

    def text(value)
    end

    def start_element(name, attrs)
    end

    def end_element(name)
    end
    
  end # Sax
end # Ox
