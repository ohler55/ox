require 'ox'

module Weather
  class WSax < ::Ox::Sax
    attr_accessor :highs

    def self.parse(filename, as_time)
      handler = self.new(as_time)
      input = IO.open(IO.sysopen(filename))
      Ox.sax_parse(handler, input)
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

    def start_element(name)
      case name
      when :Cell
        @cell += 1
      when :Row
        @row += 1
        @cell = 0
      end
    end

    def value(v)
      if 1 < @row
        case @cell
        when 2
          if @as_time
            @time = v.as_time
          else
            @time = v.as_s
          end
        when 4
          @highs[@time] = v.as_f
        end
      end
    end

    def error(message, line, column)
      puts message
    end
  end # WSax
end # Weather
