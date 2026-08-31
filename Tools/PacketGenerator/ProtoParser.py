
class ProtoParser():
    # 생성자? 선언, 첫인자로 self를 반드시 넣어야한다고 함
    def __init__(self, start_id, recv_prefix, send_prefix):
        # self == this
        # 여기서 초기화와 동시에 클래스 내부의 멤버 변수를 선언
        self.recv_pkt = [] # 수신 패킷 목록
        self.send_pkt = [] # 송신 패킷 목록
        self.total_pkt = [] # 모든 패킷 목록
        self.start_id = start_id
        self.id = start_id
        self.recv_prefix = recv_prefix
        self.send_prefix = send_prefix

    # 파일 입출력을 사용해서 .proto 파일을 열고 파싱 후 저장
    def parse_proto(self, path):
        # .proto 파일 오픈(파일 입출력 개 쉽게 만들어 놨네;;)
        # 윈도우 기본 값 인코딩 방식으로 읽어들이기 때문에 cp949로 읽어서 자꾸 에러남, 명시적으로 utf-8로 로드
        f = open(path, 'r', encoding='utf-8')
        lines = f.readlines()

        for line in lines:
            # 패킷 정의부의 시작은 message 키워드를 찾기, treu, false는 맨 앞이 대문자로 시작
            if line.startswith('message') == False:
                continue

            # message S_TEST
            # 파싱해서 message 다음 구문(패킷 이름)을 대문자로 통일
            pkt_name = line.split()[1].upper()
            # 패킷 이름이 recv_prefix(C_)로 시작하면 recv 패킷으로 분류
            if pkt_name.startswith(self.recv_prefix):
                self.recv_pkt.append(Packet(pkt_name, self.id))
            # 패킷 이름이 send_prefix(S_)로 시작하면 send 패킷으로 분류
            elif pkt_name.startswith(self.send_prefix):
                self.send_pkt.append(Packet(pkt_name, self.id))
            else:
                continue

            # 토탈 패킷에도 등록
            self.total_pkt.append(Packet(pkt_name, self.id))
            # 패킷의 id는 순서대로 1씩 증가하게 부여
            self.id += 1

        f.close()

class Packet:
    def __init__(self, name, id):
        self.name = name
        self.id = id