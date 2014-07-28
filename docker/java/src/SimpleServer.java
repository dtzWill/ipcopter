import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;

public class SimpleServer {
	public static void main(String[] args) throws Exception {
		int iters = 1000 * 1000;
		ServerSocket server = new ServerSocket(6666);
		Socket socket = server.accept();
		server.close();

		OutputStream output = socket.getOutputStream();

		byte[] bytes = new byte[10 * 1024]; // 10K
		for (int i = 0; i < bytes.length; i++) { bytes[i] = 12; } // fill the bytes

		// send them again and again
		for (int i = 0; i < iters; ++i) {
			output.write(bytes);
		}
	}
}
