from http.server import BaseHTTPRequestHandler, HTTPServer
# import logging
from urllib.parse import parse_qs
from os import path
import json

class MyHandler(BaseHTTPRequestHandler):
    def _set_response(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        
    def _get_path(self):
        if self.path == '/':
            content_path = path.join('./data/', 'default.html')
        else:
            content_path = path.join('./data/', str(self.path).split('?')[0][1:])
        return content_path
        
    def _get_content(self, content_path):
        with open(content_path, mode='r', encoding='utf-8') as f:
            content = f.read()
        return bytes(content, 'utf-8')
        
    def _load_data(self):
        if path.exists('./data/data.txt'):            
            with open('./data/data.txt', 'r') as f:
                return json.load(f)
        return None
        
    def _save_data(self, data):
        with open('./data/data.txt', 'w') as f:
            json.dump(data, f)
        
    def do_GET(self):
        if self.path == '/getranges':
            jsonData = self._load_data()
            if jsonData == None:
                data = "{\"minTemperature\": 0, \"maxTemperature\": 35, \"minHumidity\": 10, \"maxHumidity\": 90}"
            else:
                (minT, maxT, minH, maxH) = (jsonData['minTemperature'][0], jsonData['maxTemperature'][0], jsonData['minHumidity'][0], jsonData['maxHumidity'][0])
                data = '{}\"minTemperature\": {}, \"maxTemperature\": {}, \"minHumidity\": {}, \"maxHumidity\": {}{}'.format('{', minT, maxT, minH, maxH, '}')
                
            self.send_response(200)
            self.send_header('Content-type', 'text/json')
            content = data.encode('utf-8')
            # logging.info('GET data {}\nencoded {}'.format(data, content))
            self.send_header('Content-Length', len(content))
            self.end_headers()
            self.wfile.write(content)
        else:
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.send_header('Content-Length', path.getsize(self._get_path()))
            self.end_headers()
            self.wfile.write(self._get_content(self._get_path()))
        
    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        args = parse_qs(post_data.decode('utf-8'))
        logging.info('POST request\nargs: {} {}'.format(str(args), type(args)))
        self._save_data(args)
        
def run(server_class=HTTPServer, handler_class=MyHandler, port=8080):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info('Starting httpd...\n')
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('Stopping httpd...\n')
        
if __name__ == '__main__':
    from sys import argv
    
    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()