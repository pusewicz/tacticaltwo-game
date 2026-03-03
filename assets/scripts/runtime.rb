module Kernel
  def sleep(seconds)
    elapsed = 0.0
    while elapsed < seconds
      Fiber.yield
      elapsed += Game.dt
    end
  end
end
