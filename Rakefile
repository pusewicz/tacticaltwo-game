# frozen_string_literal: true

namespace :cmake do
  desc "Configure CMake RelWithDebInfo build with Ninja"
  task :configure do
    if File.exist?("build/relwithdebinfo/build.ninja")
      puts "Build already configured (build/relwithdebinfo/build.ninja exists)"
    else
      sh "cmake -B build/relwithdebinfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_HOT_RELOADING=ON"
    end
  end

  desc "Build RelWithDebInfo configuration"
  task build: :configure do
    sh "cmake --build build/relwithdebinfo"
  end
end

desc "Build RelWithDebInfo configuration (default)"
task default: "cmake:build"

desc "Run the game"
task :run do
  sh "ninja -C build/relwithdebinfo"
  exec "build/relwithdebinfo/bin/TacticalTwo.app/Contents/MacOS/TacticalTwo"
end

desc "Format C source files"
task :format do
  files = FileList["src/**/*.c", "src/**/*.h", "include/**/*.c", "include/**/*.h"]
  sh "clang-format", "-i", *files unless files.empty?
end

def notify(message, title: "TacticalTwo", sound: nil)
  return unless RUBY_PLATFORM.include?("darwin")

  script = %(display notification "#{message}" with title "#{title}")
  script += %( sound name "#{sound}") if sound
  system("osascript", "-e", script)
end

desc "Watch src/ for changes and rebuild game library"
task watch: "cmake:configure" do
  require "listen"

  listener = Listen.to("src", only: /\.(c|h)$/) do |modified, added, removed|
    puts "\nChanges detected, rebuilding game..."
    notify("Building...")
    if system("ninja -C build/relwithdebinfo game")
      notify("Build succeeded", sound: "Glass")
    else
      notify("Build failed!", sound: "Basso")
    end
  end

  puts "Watching src/ for changes... (Ctrl+C to stop)"
  listener.start
  sleep
end
