require 'time'
require 'nokogiri'

module Weather
  class WNo

    def self.parse(filename, as_time)
      highs = {}
      doc = Nokogiri::XML::Document.parse(File.open(filename))
      #table = doc.xpath('/Workbook/Worksheet/Table') # fails to return any nodes
      ws = nil
      table = nil
      doc.root.elements.each do |e|
        if 'Worksheet' == e.name
          ws = e
          break
        end
      end
      ws.elements.each do |e|
        if 'Table' == e.name
          table = e
          break
        end
      end
      first = true
      table.elements.each do |row|
        next unless 'Row' == row.name

        if first
          first = false
          next
        end
        t = row.elements[1].child().child().to_s
        high = row.elements[3].child().child().to_s.to_f
        if as_time
          highs[Time.parse(t)] = high
        else
          highs[t] = high
        end
      end
      #puts highs
      highs
    end

  end # WNo
end # Weather
