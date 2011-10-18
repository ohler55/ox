module Ox

  # 
  class InvalidPath < Exception
    def initialize(path)
      super("#{path.join('/')} is not a valid location.")
    end

  end # InvalidPath
end # Ox
