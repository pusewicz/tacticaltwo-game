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

