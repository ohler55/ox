require 'time'
require 'nokogiri'

module Weather
  class WNoSax < ::Nokogiri::XML::SAX::Document
    attr_accessor :highs

    def self.parse(filename, as_time)
      handler = self.new(as_time)
      nohand = Nokogiri::XML::SAX::Parser.new(handler)
      input = IO.open(IO.sysopen(filename))
      nohand.parse(input)
      input.close
      # puts handler.highs
      handler.highs
    end

    def initialize(as_time)
      @as_time = as_time
      @highs = {}
      @time = nil
      @row = 0
      @cell = 0
    end

    def start_element(name, attrs = [])
      case name
      when 'Cell'
        @cell += 1
      when 'Row'
        @row += 1
        @cell = 0
      end
    end

    def characters(text)
      text.strip!
      return if text.empty?

      if 1 < @row
        case @cell
        when 2
          if @as_time
            # @time = Time.parse(text)
            @time = Time.at(text.to_f)
          else
            @time = text
          end
        when 4
          @highs[@time] = text.to_f
        end
      end
    end

    def error(message)
      puts message
    end

    def warning(message)
      puts message
    end
  end # WNoSax
end # Weather
