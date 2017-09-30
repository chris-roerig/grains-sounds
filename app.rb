require 'fileutils'

file = ARGV[0]

if file.nil?
  puts "no sound file provided"
  exit
end

name = File.basename(file, ".*")
name = "a#{name}" if name[0].match(/[0-9]/)
outpath = "./patches/#{name}"
soundfile = "#{outpath}/#{name}.wav"
hfile = "#{outpath}/sample.h"
readme = "#{outpath}/README.md"

# copy original template to destination
FileUtils.cp_r 'source', outpath
FileUtils.cp file, outpath

# convert wav to usable bit and format
`sox #{file} -t u8 -c 1 -r 8000 #{soundfile}`
`xxd -i #{soundfile} > #{hfile}`

# required in sample.h
sampleh = <<~TEXT
#define SAMPLE_RATE 8000
const int sound_length=#{File.new(soundfile).size};
const unsigned char sound_data[] PROGMEM= {
TEXT

# update sample.h file
lines = File.readlines(hfile)
lines[0] = sampleh.chomp << $/
File.open(hfile, 'w') { |f| f.write(lines.join) }

# rename ino file
File.rename("#{outpath}/framen.ino", "#{outpath}/#{name}.ino")

# update readme
lines = File.readlines(readme)
lines[0] = name.upcase << $/
File.open(readme, 'w') { |f| f.write(lines.join) }

