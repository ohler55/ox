require 'time'
require 'libxml'

module Weather
  class WLx

    def self.parse(filename, as_time)
      highs = {}
      doc = LibXML::XML::Document.file(filename)
      ws = nil
      table = nil
      doc.root.each_element do |e|
        if 'Worksheet' == e.name
          ws = e
          break
        end
      end
      ws.each_element do |e|
        if 'Table' == e.name
          table = e
          break
        end
      end
      first = true
      table.each_element do |row|
        next unless 'Row' == row.name

        if first
          first = false
          next
        end
        cnt = 0
        t = nil
        row.each_element do |cell|
          cnt += 1
          case cnt
          when 2
            t = cell.first.first.to_s
          when 4
            high = cell.first.first.to_s.to_f
            if as_time
              highs[Time.parse(t)] = high
            else
              highs[t] = high
            end
            break
          end
        end
      end
      #puts highs
      highs
    end

  end # Wlx
end # Weather
