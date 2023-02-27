require 'time'
require 'ox'

module Weather
  class WOx

    def self.parse(filename, as_time)
      highs = {}
      doc = Ox.load_file(filename)
      table = doc.locate('Workbook/Worksheet/Table')[0]
      table.nodes[19..-1].each do |row| # skips Column elements and first Row
        t = row.nodes[1].nodes[0].nodes[0]
        high = row.nodes[3].nodes[0].nodes[0].to_f
        if as_time
          highs[Time.parse(t)] = high
        else
          highs[t] = high
        end
      end
      highs
    end

  end # WOx
end # Weather
