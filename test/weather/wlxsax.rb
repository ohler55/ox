
require 'time'
require 'libxml'

module Weather
  class WLxSax
    include LibXML::XML::SaxParser::Callbacks

    attr_accessor :highs

    def self.parse(filename, as_time)
      handler = self.new(as_time)
      input = IO.open(IO.sysopen(filename))
      parser = LibXML::XML::SaxParser.io(input)
      parser.callbacks = handler
      parser.parse
      input.close
      #puts handler.highs
      handler.highs
    end

    def initialize(as_time)
      @as_time = as_time
      @highs = {}
      @time = nil
      @row = 0
      @cell = 0
    end

    def on_start_element(name, attributes)
      case name
      when 'Cell'
        @cell += 1
      when 'Row'
        @row += 1
        @cell = 0
      end
    end

    def on_characters(chars)
      chars.strip!
      return if chars.empty?

      if 1 < @row
        case @cell
        when 2
          if @as_time
            @time = Time.parse(chars)
          else
            @time = chars
          end
        when 4
          @highs[@time] = chars.to_f
        end
      end
    end

    def on_error(msg)
      puts msg
    end

  end # WLxSax
end # Weather
