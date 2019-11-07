{
  "targets": [
    {
      "target_name": "pos",
      "sources": [ "src/pos.cc" ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")>"
      ]
    }
  ]
}
