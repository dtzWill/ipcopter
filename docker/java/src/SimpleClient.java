import java.net.ServerSocket;
import java.net.Socket;
import java.io.InputStream;

public class SimpleClient {

	public static void main(String[] args) throws Exception {
		Socket socket = new Socket("127.0.0.1", 6666);
		InputStream input = socket.getInputStream();
		long total = 0;
		long start = System.currentTimeMillis();

		byte[] bytes = new byte[10240]; // 10K

		// read the data again and again

		long iters = 1000 * 1000;
		long goal = iters * bytes.length;
		while (total < goal) {
			int read = input.read(bytes);
			total += read;
		}
		long cost = System.currentTimeMillis() - start;
		System.out.println("Read " + total + " bytes, speed: " + (total / (1024.0*1024)) / (cost / 1000.0) + " MB/s");
	}
}
