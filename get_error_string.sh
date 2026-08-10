#!/bin/bash


dir='src include'

lines=$(sed 's|^\s+return std::unexpected|' $(find $dir -type f ))

echo "$lines"
