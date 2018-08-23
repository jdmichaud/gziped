# -*- coding: utf-8 -*-
#test on python 3.4 ,python of lower version  has different module organization.
import http.server
from http.server import HTTPServer, BaseHTTPRequestHandler
import socketserver

PORT = 8000

Handler = http.server.SimpleHTTPRequestHandler

Handler.extensions_map = {
  '.html': 'text/html',
  '.js':  'application/x-javascript',
  '.wasm':  'application/wasm',
  '': 'application/octet-stream', # Default
}

httpd = socketserver.TCPServer(('0.0.0.0', PORT), Handler)

print('serving at port', PORT)
httpd.serve_forever()
