# frozen_string_literal: true

namespace :cmake do
  desc "Configure CMake debug build with Ninja"
  task :configure do
    if File.exist?("build/debug/build.ninja")
      puts "Build already configured (build/debug/build.ninja exists)"
    else
      sh "cmake -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_HOT_RELOADING=ON"
    end
  end

  desc "Build debug configuration"
  task build: :configure do
    sh "cmake --build build/debug"
  end
end

desc "Build debug configuration (default)"
task default: "cmake:build"

desc "Run the game"
task :run do
  sh "ninja -C build/debug"
  exec "build/debug/bin/TacticalTwo.app/Contents/MacOS/TacticalTwo"
end

desc "Format C source files"
task :format do
  files = FileList["src/**/*.c", "src/**/*.h", "include/**/*.c", "include/**/*.h"]
  sh "clang-format", "-i", *files unless files.empty?
end

