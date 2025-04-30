#pragma once

#include "../base64.hpp" // Assuming base64 utility exists here
#include "hmac_sha256.hpp"
#include "pbkdf2_hmac_sha256.hpp"
#include "sha256.hpp"

#include <algorithm>
#include <iterator> // For std::distance
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace moon {
namespace crypto {
    namespace scram {

        // --- Helper Functions ---

        // Generate a random nonce string (printable ASCII characters)
        inline std::string generate_nonce(size_t length = 24) {
            std::string nonce_chars =
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            std::random_device random_device;
            std::mt19937 generator(random_device());
            std::uniform_int_distribution<> distribution(0, nonce_chars.length() - 1);

            std::string nonce;
            nonce.reserve(length);
            for (size_t i = 0; i < length; ++i) {
                nonce += nonce_chars[distribution(generator)];
            }
            return nonce;
        }

        // Parse SCRAM message attributes (e.g., "n=user,r=nonce") into a map
        inline std::map<char, std::string> parse_scram_attributes(const std::string& message) {
            std::map<char, std::string> attributes;
            std::stringstream ss(message);
            std::string segment;

            while (std::getline(ss, segment, ',')) {
                if (segment.length() >= 3 && segment[1] == '=') {
                    attributes[segment[0]] = segment.substr(2);
                } else if (segment.length() == 1)
                { // Handle single character flags like 'm' (mandatory extension)
                    attributes[segment[0]] = ""; // Assign empty string to indicate presence
                }
                // Silently ignore malformed segments
            }
            return attributes;
        }

        // SASLprep (simplified - assumes valid UTF-8 input, no normalization/prohibited chars check)
        // SCRAM requires username/password normalization, but a full implementation is complex.
        // This placeholder replaces ',' with '=2C' and '=' with '=3D'.
        // WARNING: A proper SASLprep implementation is needed for full compliance and security.
        inline std::string saslprep_simple_placeholder(const std::string& input) {
            std::string output;
            output.reserve(input.size());
            for (char c: input) {
                if (c == ',') {
                    output += "=2C";
                } else if (c == '=') {
                    output += "=3D";
                } else {
                    output += c;
                }
            }
            return output;
        }

        // XOR two byte vectors
        inline std::vector<uint8_t>
        xor_vectors(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
            size_t len = std::min(a.size(), b.size());
            std::vector<uint8_t> result(len);
            for (size_t i = 0; i < len; ++i) {
                result[i] = a[i] ^ b[i];
            }
            return result;
        }

        inline std::vector<uint8_t> xor_arrays(
            const std::array<uint8_t, detail::SHA256_DIGEST_SIZE>& a,
            const std::array<uint8_t, detail::SHA256_DIGEST_SIZE>& b
        ) {
            std::vector<uint8_t> result(detail::SHA256_DIGEST_SIZE);
            for (size_t i = 0; i < detail::SHA256_DIGEST_SIZE; ++i) {
                result[i] = a[i] ^ b[i];
            }
            return result;
        }

        // --- SCRAM-SHA-256 Client Implementation ---

        class ScramSha256Client {
        public:
            enum class State { INITIAL, FIRST_SENT, FINAL_SENT, AUTHENTICATED, ERROR };

            ScramSha256Client(std::string username, std::string password):
                username_(username), // REMOVED saslprep_simple_placeholder for testing
                password_(password), // Password is used directly in PBKDF2, not SASLprepped here
                state_(State::INITIAL) {}

            // Generates the client-first message bare part ("n=...,r=...")
            std::string prepare_first_message() {
                if (state_ != State::INITIAL) {
                    throw std::runtime_error("Invalid state for preparing first message.");
                }
                client_nonce_ = generate_nonce();
                client_first_message_bare_ = "n=" + username_ + ",r=" + client_nonce_;
                state_ = State::FIRST_SENT;
                // Return only the bare message part
                return client_first_message_bare_;
            }

            // Processes the server-first message ("r=...,s=...,i=...")
            // Computes SaltedPassword, ClientKey, ServerKey, StoredKey
            void process_server_first(const std::string& server_first_message) {
                if (state_ != State::FIRST_SENT) {
                    throw std::runtime_error("Invalid state for processing server first message.");
                }

                auto attributes = parse_scram_attributes(server_first_message);

                auto r_it = attributes.find('r');
                auto s_it = attributes.find('s');
                auto i_it = attributes.find('i');

                if (r_it == attributes.end() || s_it == attributes.end()
                    || i_it == attributes.end())
                {
                    state_ = State::ERROR;
                    throw std::runtime_error(
                        "Server first message missing required attributes (r, s, i)."
                    );
                }

                server_nonce_ = r_it->second;
                if (server_nonce_.rfind(client_nonce_, 0) != 0)
                { // Check if server nonce starts with client nonce
                    state_ = State::ERROR;
                    throw std::runtime_error("Server nonce does not match client nonce prefix.");
                }

                std::string salt_b64 = s_it->second;
                iterations_ = std::stoul(i_it->second); // Use stoul for unsigned long

                if (iterations_ == 0) {
                    state_ = State::ERROR;
                    throw std::runtime_error("Server iterations count cannot be zero.");
                }

                // Decode salt from base64
                salt_ = moon::base64_decode(salt_b64);
                if (salt_.empty() && !salt_b64.empty()) { // Check for decoding errors
                    state_ = State::ERROR;
                    throw std::runtime_error("Failed to decode salt from base64.");
                }

                // --- Key Derivation ---
                // SaltedPassword := Hi(Normalize(password), salt, i)
                // Store as raw bytes
                salted_password_ =
                    pbkdf2_hmac_sha256(password_, salt_, iterations_, detail::SHA256_DIGEST_SIZE);

                // ClientKey := HMAC(SaltedPassword, "Client Key")
                const std::string client_key_str = "Client Key";
                client_key_ = hmac_sha256(
                    salted_password_.data(),
                    salted_password_.size(),
                    reinterpret_cast<const uint8_t*>(client_key_str.data()),
                    client_key_str.size()
                );

                // StoredKey := H(ClientKey)
                stored_key_ = sha256(client_key_.data(), client_key_.size());

                // ServerKey := HMAC(SaltedPassword, "Server Key")
                // (Server computes this, client doesn't strictly need it unless verifying server sig later)
                // server_key_ = hmac_sha256(salted_password_, std::string("Server Key"));

                // Prepare for next step
                auth_message_ = client_first_message_bare_ + "," + server_first_message;
                // state_ remains FIRST_SENT until client final is prepared
            }

            // Generates the client-final message ("c=...,r=...,p=...")
            std::string prepare_final_message() {
                if (state_ != State::FIRST_SENT || auth_message_.empty()) {
                    throw std::runtime_error(
                        "Invalid state or missing data for preparing final message."
                    );
                }

                // Channel binding data: base64 encoding of gs2-header "n,," (no authzid, no channel binding)
                std::string channel_binding_data = moon::base64_encode("n,,"); // "n,," -> "bixz"
                client_final_message_without_proof_ =
                    "c=" + channel_binding_data + ",r=" + server_nonce_;

                // --- Proof Calculation ---
                // AuthMessage := client-first-message-bare + "," + server-first-message + "," + client-final-message-without-proof
                std::string full_auth_message =
                    auth_message_ + "," + client_final_message_without_proof_;

                // ClientSignature := HMAC(StoredKey, AuthMessage)
                std::array<uint8_t, detail::SHA256_DIGEST_SIZE> client_signature = hmac_sha256(
                    stored_key_.data(),
                    stored_key_.size(),
                    reinterpret_cast<const uint8_t*>(full_auth_message.data()),
                    full_auth_message.size()
                );

                // ClientProof := ClientKey XOR ClientSignature
                std::vector<uint8_t> client_proof_vec = xor_arrays(client_key_, client_signature);

                // Proof := base64(ClientProof)
                std::string proof =
                    moon::base64_encode(client_proof_vec.data(), client_proof_vec.size());
                state_ = State::FINAL_SENT;
                return client_final_message_without_proof_ + ",p=" + proof;
            }

            // Processes the server-final message ("v=...")
            void process_server_final(const std::string& server_final_message) {
                if (state_ != State::FINAL_SENT) {
                    throw std::runtime_error("Invalid state for processing server final message.");
                }

                auto attributes = parse_scram_attributes(server_final_message);
                auto v_it = attributes.find('v');

                if (v_it == attributes.end()) {
                    state_ = State::ERROR;
                    throw std::runtime_error(
                        "Server final message missing required attribute 'v'."
                    );
                }

                std::string server_signature_b64 = v_it->second;
                auto server_signature = moon::base64_decode(server_signature_b64);
                if (server_signature.empty() && !server_signature_b64.empty()) {
                    state_ = State::ERROR;
                    throw std::runtime_error("Failed to decode server signature from base64.");
                }

                // --- Server Signature Verification ---
                // ServerKey := HMAC(SaltedPassword, "Server Key")
                const std::string server_key_str = "Server Key";
                std::array<uint8_t, detail::SHA256_DIGEST_SIZE> server_key = hmac_sha256(
                    salted_password_.data(),
                    salted_password_.size(),
                    reinterpret_cast<const uint8_t*>(server_key_str.data()),
                    server_key_str.size()
                );

                // ExpectedServerSignature := HMAC(ServerKey, AuthMessage)
                // AuthMessage was: client-first-message-bare + "," + server-first-message + "," + client-final-message-without-proof
                std::string full_auth_message =
                    auth_message_ + "," + client_final_message_without_proof_;
                std::array<uint8_t, detail::SHA256_DIGEST_SIZE> expected_server_signature =
                    hmac_sha256(
                        server_key.data(),
                        server_key.size(),
                        reinterpret_cast<const uint8_t*>(full_auth_message.data()),
                        full_auth_message.size()
                    );

                // Constant-time comparison is recommended here, but std::equal is used for simplicity.
                // WARNING: Vulnerable to timing attacks. Use a secure comparison function in production.
                if (server_signature.size() == expected_server_signature.size()
                    && std::equal(
                        server_signature.begin(),
                        server_signature.end(),
                        expected_server_signature.begin()
                    ))
                {
                    state_ = State::AUTHENTICATED;
                } else {
                    state_ = State::ERROR;
                    throw std::runtime_error("Server signature verification failed.");
                }
            }

            State get_state() const {
                return state_;
            }
            bool is_authenticated() const {
                return state_ == State::AUTHENTICATED;
            }
            bool is_error() const {
                return state_ == State::ERROR;
            }

        private:
            std::string username_;
            std::string password_; // Store original password for PBKDF2
            std::string client_nonce_;
            std::string server_nonce_;
            std::vector<uint8_t> salt_;
            uint32_t iterations_ = 0;

            std::string client_first_message_bare_;
            std::string auth_message_; // Combined messages for signature calculation
            std::string client_final_message_without_proof_;

            // Derived keys
            std::vector<uint8_t> salted_password_; // Store as raw bytes
            std::array<uint8_t, detail::SHA256_DIGEST_SIZE> client_key_;
            std::array<uint8_t, detail::SHA256_DIGEST_SIZE> stored_key_;
            // std::array<uint8_t, detail::SHA256_DIGEST_SIZE> server_key_; // Only needed transiently for verification

            State state_;
        };
    } // namespace scram
} // namespace crypto
} // namespace moon
