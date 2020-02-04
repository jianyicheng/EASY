# installing mono (required by Boogie)
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
echo "deb http://download.mono-project.com/repo/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mono-xamarin.list
sudo apt-get update
sudo apt-get install mono-complete
sudo apt-get install monodevelop
sudo apt-get install monodevelop-nunit

# installing z3 (required by Boogie)
git clone https://github.com/Z3Prover/z3.git
cd z3/
python scripts/mk_make.py --prefix=/home/vagrant
cd build
make
sudo make install
cd ../..

# installing Boogie
git clone https://github.com/boogie-org/boogie.git
sudo apt install nuget
cd boogie
nuget restore Source/Boogie.sln
msbuild Source/Boogie.sln
cd Binaries
sudo ln -s ../../z3/build/z3 z3
cd ..

# installing LLVM pass
cp -r easy ~/legup/llvm/lib/Transforms/
if ! grep -q "add_subdirectory(easy)" ~/legup/llvm/lib/Transforms/CMakeLists.txt; then
  echo "add_subdirectory(EASY)" >> ~/legup/llvm/lib/Transforms/CMakeLists.txt
fi
touch ~/legup/llvm/lib/Transforms/easy/EASY.cpp
cd ~/legup/llvm/lib/Transforms/easy/
make

# You need to add your boogie and z3 in your PATH environement variable so the bash would work
