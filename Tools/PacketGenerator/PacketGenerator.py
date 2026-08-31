import argparse
import jinja2
import ProtoParser
import sys

def main():

    # argparse 선언, 문자열 선언을 작은 다운표, 큰 다운표 양쪽 전부로 가능
    arg_parser = argparse.ArgumentParser(description = 'PacketGenerator')
    # 파싱을 위해 실행시 cmd 라인드로 받을 argument추가
    # 파싱할 .proto파일의 경로를 path에 저장
    # 경로에 백슬래시(\) 하나만 사용하면 유니코드 이스케이프로 인식하므로 /나 \\ 혹은 경로 앞에 r을 붙혀서 로우 문자열로 처리해야 함
    arg_parser.add_argument('--path', type=str, default=r'C:\Users\Chibi\Documents\Server\Server\Common\Protobuf\Protocol.proto', help='proto path')
    # 출력물 이름을 output에 저장
    arg_parser.add_argument('--output', type=str, default='TestPacketHandler', help='output file')
    # recv 패킷 정의부를 찾기위한 convention을 recv로 저장
    arg_parser.add_argument('--recv', type=str, default='C_', help='recv convention')
    # send 패킷 정의부를 찾기위한 convention을 send로 저장
    arg_parser.add_argument('--send', type=str, default='S_', help='send convention')
    args = arg_parser.parse_args()

    # ProtoParser 생성
    parser = ProtoParser.ProtoParser(1000, args.recv, args.send)
    parser.parse_proto(args.path)
    # jinja2를 사용해서 Templates 파일 로드
    file_loader = jinja2.FileSystemLoader('Templates')
    env = jinja2.Environment(loader=file_loader)

    # template이 될 PacketHandler.h를 로드
    template = env.get_template('PacketHandler.h')
    # jinja2를 사용해서 템플릿에 맞게 코드 생성, 인자로 .proto를 파싱한 결과와 결과물 출력을 위한 output argument를 전달
    output = template.render(parser=parser, output=args.output)

    # 여기도 utf-8 설정
    f = open(args.output+'.h', 'w+', encoding='utf-8')
    f.write(output)
    f.close()

    sys.stdout.reconfigure(encoding='utf-8')
    print(output)
    return

if __name__=='__main__':
    main()