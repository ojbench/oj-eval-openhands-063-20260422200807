#ifndef BPT_MEMORYRIVER_HPP
#define BPT_MEMORYRIVER_HPP

#include <fstream>
#include <string>

using std::string;
using std::fstream;
using std::ifstream;
using std::ofstream;

// A simple file-backed storage with free-list reclamation.
// Header layout (ints):
// [0 .. info_len-1]   -> user-visible info ints (1-based in API)
// [info_len]          -> free list head offset (0 means empty)
// Objects T are stored contiguously after the header. Deleted slots form a singly linked list
// using the first int-sized bytes of the slot as the "next" pointer.

template<class T, int info_len = 2>
class MemoryRiver {
private:
    fstream file;
    string file_name;
    int sizeofT = sizeof(T);

    static_assert(sizeof(T) >= sizeof(int), "T must be at least the size of int to store free list links");

    inline std::streamoff header_int_pos(int n_1_base) const {
        return static_cast<std::streamoff>((n_1_base - 1) * sizeof(int));
    }

    inline std::streamoff user_header_bytes() const {
        return static_cast<std::streamoff>(info_len * sizeof(int));
    }

    inline std::streamoff total_header_bytes() const {
        // Extra 1 int for free-list head
        return static_cast<std::streamoff>((info_len + 1) * sizeof(int));
    }

    // Position (byte offset) of the extra free list head int in header
    inline std::streamoff free_head_pos() const {
        return header_int_pos(info_len + 1); // 1-based index = info_len + 1
    }

    int get_free_head() {
        open_file(std::ios::in | std::ios::out | std::ios::binary);
        file.seekg(free_head_pos(), std::ios::beg);
        int head = 0;
        file.read(reinterpret_cast<char*>(&head), sizeof(int));
        close_file();
        return head;
    }

    void set_free_head(int head) {
        open_file(std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(free_head_pos(), std::ios::beg);
        file.write(reinterpret_cast<char*>(&head), sizeof(int));
        file.flush();
        close_file();
    }

    void open_file(std::ios::openmode mode) {
        file.open(file_name, mode);
    }

    void close_file() {
        if (file.is_open()) file.close();
    }

public:
    MemoryRiver() = default;

    MemoryRiver(const string& fn) : file_name(fn) {}

    void initialise(string FN = "") {
        if (FN != "") file_name = FN;
        // Create/truncate binary file and initialize header ints to 0.
        file.open(file_name, std::ios::out | std::ios::binary | std::ios::trunc);
        int tmp = 0;
        // Write user header
        for (int i = 0; i < info_len; ++i) file.write(reinterpret_cast<char*>(&tmp), sizeof(int));
        // Extra int for free list head
        file.write(reinterpret_cast<char*>(&tmp), sizeof(int));
        file.close();
    }

    // Read the n-th (1-based) user info int into tmp
    void get_info(int &tmp, int n) {
        if (n > info_len || n <= 0) return;
        open_file(std::ios::in | std::ios::binary);
        file.seekg(header_int_pos(n), std::ios::beg);
        file.read(reinterpret_cast<char*>(&tmp), sizeof(int));
        close_file();
    }

    // Write tmp into the n-th (1-based) user info int
    void write_info(int tmp, int n) {
        if (n > info_len || n <= 0) return;
        open_file(std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(header_int_pos(n), std::ios::beg);
        file.write(reinterpret_cast<char*>(&tmp), sizeof(int));
        file.flush();
        close_file();
    }

    // Write object t to a suitable position and return its byte offset index
    int write(T &t) {
        int head = get_free_head();
        if (head != 0) {
            // Reuse a freed slot at offset 'head'
            // Read next pointer from first 4 bytes of this slot
            int next = 0;
            open_file(std::ios::in | std::ios::out | std::ios::binary);
            file.seekg(static_cast<std::streamoff>(head), std::ios::beg);
            file.read(reinterpret_cast<char*>(&next), sizeof(int));
            // Update free list head
            close_file();
            set_free_head(next);
            // Write object into this slot
            open_file(std::ios::in | std::ios::out | std::ios::binary);
            file.seekp(static_cast<std::streamoff>(head), std::ios::beg);
            file.write(reinterpret_cast<char*>(&t), sizeof(T));
            file.flush();
            close_file();
            return head;
        } else {
            // Append at end
            open_file(std::ios::in | std::ios::out | std::ios::binary);
            // If file doesn't exist yet (no header), create and init
            if (!file.is_open()) {
                close_file();
                initialise(file_name);
                open_file(std::ios::in | std::ios::out | std::ios::binary);
            }
            file.seekp(0, std::ios::end);
            std::streamoff pos = file.tellp();
            file.write(reinterpret_cast<char*>(&t), sizeof(T));
            file.flush();
            close_file();
            return static_cast<int>(pos);
        }
    }

    // Overwrite the object at byte offset index with t
    void update(T &t, const int index) {
        open_file(std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(static_cast<std::streamoff>(index), std::ios::beg);
        file.write(reinterpret_cast<char*>(&t), sizeof(T));
        file.flush();
        close_file();
    }

    // Read the object at byte offset index into t
    void read(T &t, const int index) {
        open_file(std::ios::in | std::ios::binary);
        file.seekg(static_cast<std::streamoff>(index), std::ios::beg);
        file.read(reinterpret_cast<char*>(&t), sizeof(T));
        close_file();
    }

    // Delete the object at byte offset index (push it onto free list)
    void Delete(int index) {
        int head = get_free_head();
        // Write current head into first 4 bytes of this slot
        open_file(std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(static_cast<std::streamoff>(index), std::ios::beg);
        file.write(reinterpret_cast<char*>(&head), sizeof(int));
        file.flush();
        close_file();
        // Update free list head to this index
        set_free_head(index);
    }
};


#endif //BPT_MEMORYRIVER_HPP
